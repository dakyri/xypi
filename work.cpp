#include "work.h"

#include "crypttools.h"
#include "dongle.h"
#include "jsonutil.h"
#include "rsakeys.h"

#include <boost/algorithm/hex.hpp>
#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using spdlog::info;
using spdlog::debug;

/*!
 * \class EncryptWork work.h
 * encapsulates a basic encryption job
 */

std::shared_ptr<work_t> EncryptWork::create(const std::string& cmd, jobid_t id, const nlohmann::json& request)
{
	const workid_t workid = jutil::opt_ull(request, "workId", 0);
	const std::string keyid = jutil::need_s(request, "KEYID");
	const std::string data = jutil::need_s(request, "DATA");

	return std::make_shared<EncryptWork>(cmd, id, workid, keyid, data);
}

EncryptWork::EncryptWork(const std::string& cmd, jobid_t id, workid_t workid, const std::string& keyid, const std::string& data)
	: work_t(cmd, id, workid), m_keyid(keyid), m_data(data)
{}

json EncryptWork::toJson()
{
	json jsonval;
	jsonval["cmd"] = cmd;
	jsonval["KEYID"] = m_keyid;
	jsonval["DATA"] = m_data;
	return jsonval;
}

std::pair<uint32_t, json> EncryptWork::process(Dongle* dongle)
{
	debug("EncryptWork::process(result {}, work {}): key {} data {}", id, workId, m_keyid, m_data);
	json response;
	if (m_rawData.empty()) {
		m_rawData.assign(m_data.length() / 2, 0);
		try {
			boost::algorithm::unhex(m_data, m_rawData.begin());
		} catch (...) {
			return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON("Invalid hex input :(")};
		}
	}

	uint32_t out_len = 256;
	std::vector<uint8_t> out(out_len);
	if (m_keyid == "MTK1") {
		if (!crypttools::rsaSignMtk1(m_rawData.data(), m_rawData.size(), out.data(), out_len))
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Mtk1 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else if (m_keyid == "MTK4") {
		if (!crypttools::rsaSignPem(m_rawData.data(), m_rawData.size(), out.data(), out_len, pemMtk4Key))
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Mtk4 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else if (m_keyid == "HwOemKeyB") {
		if (!crypttools::rsaPrivateEncryptPkcs8(m_rawData.data(), m_rawData.size(), out.data(), out_len, pemHwOemKeyB))
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Oem openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else if (m_keyid == "0x4000") {
		if (!crypttools::rsaPrivateEncryptPkcs8(m_rawData.data(), m_rawData.size(), out.data(), out_len, pemChimera4000Key))
			return {Dongle::error::OPENSSL_ERR,
					jutil::errorJSON(fmt::format("x4000 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else {
		if (!dongle) return {Dongle::error::DONGLE_REQUIRED, response};
		int ikeyid = 0;
		try {
			ikeyid = std::stoi(m_keyid, 0, 16);
		} catch (...) {
			return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON(fmt::format("Dongle encryption errror: invalid keyid {}.", m_keyid))};
		}
		auto ret = dongle->encrypt(ikeyid, m_rawData, out);
		if (ret != Dongle::error::SUCCESS) {
			return {ret, jutil::errorJSON(fmt::format("Dongle encryption error on key {}: {}", m_keyid, Dongle::errorStr(ret)))};
		}
	}
	std::string hex(out.size() * 2, 0);
	boost::algorithm::hex(out.begin(), out.end(), hex.begin());
	response["DATA"] = hex;
	return {0, response};
}

/*!
 * \class RecryptWork work.h
 * encapsulates a basic re-encryption job
 */

std::shared_ptr<work_t> RecryptWork::create(const std::string& cmd, const jobid_t id, const nlohmann::json& request)
{
	const workid_t workid = jutil::opt_ull(request, "workId", 0);
	const std::string inkeyid = jutil::need_s(request, "INKEYID");
	const std::string outkeyid = jutil::need_s(request, "OUTKEYID");
	const std::string data = jutil::need_s(request, "DATA");

	return std::make_shared<RecryptWork>(cmd, id, workid, inkeyid, outkeyid, data);
}

RecryptWork::RecryptWork(
	const std::string& cmd, jobid_t id, workid_t workid, const std::string& inkeyid, const std::string& outkeyid, const std::string& data)
	: work_t(cmd, id, workid), m_inkeyid(inkeyid), m_outkeyid(outkeyid), m_data(data)
{}

json RecryptWork::toJson()
{
	json jsonval;
	jsonval["cmd"] = cmd;
	jsonval["INKEYID"] = m_inkeyid;
	jsonval["OUTKEYID"] = m_outkeyid;
	jsonval["DATA"] = m_data;
	return jsonval;
}


std::pair<uint32_t, json> RecryptWork::process(Dongle* dongle)
{
	debug("RecryptWork::process(result {}, work {}): inkey {} outkey {} data {}", id, workId, m_inkeyid, m_outkeyid, m_data);

	json response;
	if (m_decodedData.empty()) {
		bool hashed = (cmd == "recryptHash");
		if (m_rawData.empty()) {
			m_rawData.assign(m_data.length() / 2, 0);
			try {
				boost::algorithm::unhex(m_data, m_rawData.begin());
			} catch (...) {
				return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON("Invalid hex input :(")};
			}
		}

		uint32_t decoded_len = 256;
		m_decodedData.resize(decoded_len);
		if (m_inkeyid == "MTK1" || m_inkeyid == "MTK4" || m_inkeyid == "HwOemKeyB") {
			return {Dongle::error::INVALID_PARAMETER,
					jutil::errorJSON(fmt::format("Decryption failed, unsupported input key {}! ;(", m_inkeyid))};
		} else if (m_inkeyid == "HwOemKeyC") {
			if (!crypttools::rsaPrivateDecryptPkcs8(m_rawData.data(), m_rawData.size(), m_decodedData.data(), decoded_len, pemHwOemKeyC))
				return {Dongle::error::OPENSSL_ERR,
						jutil::errorJSON(fmt::format("Oem openssl decryption failed: {}", crypttools::osslError()))};
			m_decodedData.resize(decoded_len);
		} else if (m_inkeyid == "0x4000") {
			if (!crypttools::rsaPrivateDecryptUnpadded(m_rawData.data(), m_rawData.size(), m_decodedData.data(), decoded_len,
													   pemChimera4000Key))
				return {Dongle::error::OPENSSL_ERR,
						jutil::errorJSON(fmt::format("x4000 openssl decryption failed: {}", crypttools::osslError()))};
			m_decodedData.resize(decoded_len);
		} else {
			if (!dongle) {
				m_decodedData.clear(); // so we will retry this branch when we revisit this item
				return {Dongle::error::DONGLE_REQUIRED, response};
			}
			int ikeyid = 0;
			try {
				ikeyid = std::stoi(m_inkeyid, 0, 16);
			} catch (...) { // this is a final error on this piece of work. no takesy-backsies.
				return {Dongle::error::INVALID_PARAMETER,
						jutil::errorJSON(fmt::format("Dongle decryption error: invalid inkeyid {}!", m_inkeyid))};
			}
			auto ret = dongle->decrypt(ikeyid, m_rawData, m_decodedData);
			if (ret != Dongle::error::SUCCESS) {
				m_decodedData.clear(); // so we will retry this branch if we revisit this item after a reboot on the hub
				return {ret, jutil::errorJSON(fmt::format("Dongle decryption error on key {}: {}!", m_inkeyid, Dongle::errorStr(ret)))};
			}
		}

		if (hashed) {
			std::vector<uint8_t> hash;
			hash.resize(SHA256_DIGEST_LENGTH);
			SHA256(m_decodedData.data(), m_decodedData.size(), hash.data());
			m_decodedData = hash;
		}

		if (spdlog::default_logger()->should_log(spdlog::level::debug)) {
			std::string hex1(m_decodedData.size() * 2, 0);
			boost::algorithm::hex(m_decodedData.begin(), m_decodedData.end(), hex1.begin());
			debug("cmd {}, decrypted data: {}", cmd, hex1);
		}
	}

	std::vector<uint8_t> out;
	uint32_t out_len = 256;
	out.resize(out_len);
	if (m_outkeyid == "MTK1") {
		if (!crypttools::rsaSignMtk1(m_decodedData.data(), m_decodedData.size(), out.data(), out_len))
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Mtk1 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else if (m_outkeyid == "MTK4") {
		if (!crypttools::rsaSignPem(m_decodedData.data(), m_decodedData.size(), out.data(), out_len, pemMtk4Key))
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Mtk4 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else if (m_outkeyid == "HwOemKeyB") {
		if (!crypttools::rsaPrivateEncryptPkcs8((unsigned char*)m_decodedData.data(), m_decodedData.size(), out.data(), out_len,
												pemHwOemKeyB)) {
			return {Dongle::error::OPENSSL_ERR, jutil::errorJSON(fmt::format("Oem openssl encryption failed: {}", crypttools::osslError()))};
		}
		out.resize(out_len);
	} else if (m_outkeyid == "0x4000") {
		if (!crypttools::rsaPrivateEncryptPkcs8(m_decodedData.data(), m_decodedData.size(), out.data(), out_len, pemChimera4000Key))
			return {Dongle::error::OPENSSL_ERR,
					jutil::errorJSON(fmt::format("x4000 openssl encryption failed: {}", crypttools::osslError()))};
		out.resize(out_len);
	} else {
		if (!dongle) return {Dongle::error::DONGLE_REQUIRED, response};
		int okeyid = 0;
		try {
			okeyid = std::stoi(m_outkeyid, 0, 16);
		} catch (...) {
			return {Dongle::error::INVALID_PARAMETER, fmt::format("Dongle encryption errror: invalid keyid {}.", m_outkeyid)};
		}
		auto ret = dongle->encrypt(okeyid, m_decodedData, out);
		if (ret != Dongle::error::SUCCESS) {
			return {ret, jutil::errorJSON(fmt::format("Dongle encryption error on key {}: {}", m_outkeyid, Dongle::errorStr(ret)))};
		}
	}

	std::string hex(out.size() * 2, 0);
	boost::algorithm::hex(out.begin(), out.end(), hex.begin());
	response["DATA"] = hex;
	return {0, response};
}

/*!
 * \class SignWork work.h
 * encapsulates a basic sign job and is the base class of more specialized encryption tasks
 */

/*!
 * template to make a shared_ptr to any of our SignWork descendents
 */
template<typename SWT>
std::shared_ptr<work_t> SignWork::create(const std::string& cmd, const jobid_t id, const nlohmann::json& request)
{
	const workid_t workid = jutil::opt_ull(request, "workId", 0);
	const std::string diesn = jutil::need_s(request, "DIESN");
	const std::string hash = jutil::need_s(request, "HASH");

	const auto digest_length = (cmd == "signaes" ? 16 : SHA256_DIGEST_LENGTH);

	if (diesn.length() != 42 || diesn.substr(0, 2) != "0x") throw std::invalid_argument("Invalid diesn!");

	if (hash.size() != digest_length * 2)
		throw std::invalid_argument(fmt::format("Invalid hash size {}, digest length {}!", hash.size(), digest_length));

	return std::make_shared<SWT>(cmd, id, workid, diesn, hash);
}

// we need these specific specializations of that template, and force them here to avoid linkage chaos:
template std::shared_ptr<work_t> SignWork::create<SignWork>(const std::string& cmd, jobid_t id, const nlohmann::json& req);
template std::shared_ptr<work_t> SignWork::create<SignV2Work>(const std::string& cmd, jobid_t id, const nlohmann::json& req);
template std::shared_ptr<work_t> SignWork::create<SignAesWork>(const std::string& cmd, jobid_t id, const nlohmann::json& req);

SignWork::SignWork(const std::string& cmd, jobid_t id, workid_t workid, const std::string& diesn, const std::string& hash)
	: work_t(cmd, id, workid), m_diesn(diesn), m_hash(hash)
{}

json SignWork::toJson()
{
	nlohmann::json jsonval;
	jsonval["cmd"] = cmd;
	jsonval["DIESN"] = m_diesn;
	jsonval["HASH"] = m_hash;
	return jsonval;
}

std::pair<uint32_t, json> SignWork::process(Dongle* dongle)
{
	debug("SignWork::process(result {}, work {}) hash {} diesn {}", id, workId, m_hash, m_diesn);
	json result;

	const int digest_length = SHA256_DIGEST_LENGTH;
	std::vector<uint8_t> rawHash(digest_length, 0);
	try {
		boost::algorithm::unhex(m_hash, rawHash.begin());
	} catch (...) {
		return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON("Invalid hash hex input :(")};
	}

	std::string huk;
	try {
		crypttools::dieToHuk(m_diesn, huk);
	} catch (...) {
		return {Dongle::error::INVALID_HUK, jutil::errorJSON("Failed to get HUK!")};
	}
	huk += "Secure Storage Key";
	uint8_t keyhash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<uint8_t*>(huk.data()), huk.length() /*16 + 18*/, keyhash);

	std::vector<uint8_t> sign(SHA256_DIGEST_LENGTH, 0);
	crypttools::hmacSha256(rawHash.data(), SHA256_DIGEST_LENGTH, keyhash, SHA256_DIGEST_LENGTH, sign.data());

	std::string signature(sign.size() * 2, 0);
	boost::algorithm::hex(sign.begin(), sign.end(), signature.begin());
	result["SIGNATURE"] = signature;
	return {0, result};
}

/*!
 * \class SignV2Work work.h
 * encapsulates a basic signv2 job
 * this uses one of our magical dongle slots to do the signing
 */

SignV2Work::SignV2Work(const std::string& cmd, jobid_t id, workid_t workid, const std::string& diesn, const std::string& hash)
	: SignWork(cmd, id, workid, diesn, hash)
{}

std::pair<uint32_t, json> SignV2Work::process(Dongle* dongle)
{
	debug("SignV2Work::process(result {}, work {}) hash {} diesn {}", id, workId, m_hash, m_diesn);
	json result;
	if (!dongle) return {Dongle::error::DONGLE_REQUIRED, result};

	const int digest_length = SHA256_DIGEST_LENGTH;
	std::vector<uint8_t> rawHash(digest_length, 0);
	try {
		boost::algorithm::unhex(m_hash, rawHash.begin());
	} catch (...) {
		return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON("Invalid hash hex input :(")};
	}

	std::vector<uint8_t> diehash(SHA256_DIGEST_LENGTH, 0);
	std::string diesn = m_diesn.substr(2);
	SHA256((unsigned char*)diesn.data(), diesn.size(), diehash.data());

	std::vector<uint8_t> hukv(256, 0); // allocate space for rsa dongle result
	auto ret = dongle->encrypt(0x8F06, diehash, hukv);
	if (ret != Dongle::error::SUCCESS) return {ret, jutil::errorJSON(Dongle::errorStr(ret))};
	std::string huk(hukv.begin() + 240, hukv.begin() + 240 + 16); // substr(240, 16);

	std::vector<uint8_t> keyhash(SHA256_DIGEST_LENGTH, 0);
	huk += "Secure Storage Key";
	SHA256(reinterpret_cast<uint8_t*>(huk.data()), huk.length() /*16 + 18*/, keyhash.data());

	std::vector<uint8_t> sign(SHA256_DIGEST_LENGTH, 0);
	crypttools::hmacSha256(rawHash.data(), SHA256_DIGEST_LENGTH, keyhash.data(), SHA256_DIGEST_LENGTH, sign.data());

	std::string signature(sign.size() * 2, 0);
	boost::algorithm::hex(sign.begin(), sign.end(), signature.begin());
	result["SIGNATURE"] = signature;
	return {0, result};
}

/*!
 * \class SignAesWork work.h
 * encapsulates an aes sign job
 */

SignAesWork::SignAesWork(const std::string& cmd, jobid_t id, workid_t workid, const std::string& diesn, const std::string& hash)
	: SignWork(cmd, id, workid, diesn, hash)
{}

std::pair<uint32_t, json> SignAesWork::process(Dongle* dongle)
{
	debug("SignAESWork::process(result {}, work {}) hash {} diesn {}", id, workId, m_hash, m_diesn);
	json result;
	const int digest_length = 16;
	std::vector<uint8_t> rawHash(digest_length, 0);
	try {
		boost::algorithm::unhex(m_hash, rawHash.begin());
	} catch (...) {
		return {Dongle::error::INVALID_PARAMETER, jutil::errorJSON("Invalid hash hex input :(")};
	}

	std::string huk;
	try {
		crypttools::dieToHuk(m_diesn, huk);
	} catch (...) {
		return {Dongle::error::INVALID_HUK, jutil::errorJSON("Failed to get HUK!")};
	}

	uint8_t keyhash[SHA256_DIGEST_LENGTH];
	huk += "Secure Storage Key";
	SHA256(reinterpret_cast<uint8_t*>(huk.data()), huk.length() /*16 + 18*/, keyhash);

	AES_KEY aes;
	AES_set_encrypt_key(keyhash, SHA256_DIGEST_LENGTH * 8, &aes);

	std::vector<uint8_t> sign(digest_length, 0);
	AES_ecb_encrypt(rawHash.data(), sign.data(), &aes, AES_ENCRYPT);

	std::string signature(sign.size() * 2, 0);
	boost::algorithm::hex(sign.begin(), sign.end(), signature.begin());
	result["SIGNATURE"] = signature;
	return {0, result};
}

/*!
 * \class PingWork work.h
 * this is pretty lazy as far as work goes. we ask the dongle for a random number which we then ignore.
 */

std::shared_ptr<work_t> PingWork::create(const std::string& cmd, jobid_t id, const nlohmann::json& request)
{
	return std::make_shared<PingWork>(cmd, id);
}

PingWork::PingWork(const std::string& cmd, id_t id) : work_t(cmd, id, 0) {}

nlohmann::json PingWork::toJson()
{
	nlohmann::json jsonval;
	jsonval["cmd"] = cmd;
	return jsonval;
}

std::pair<uint32_t, json> PingWork::process(Dongle* dongle)
{
	json result;
	debug("PingWork::process(result {}))", id);
	if (!dongle) return {Dongle::error::DONGLE_REQUIRED, result};
	result["usb"] = dongle->ping();
	return {0, result};
}
