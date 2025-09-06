#include "wpch.h"
#include "Guid.h"

#include "GenericPlatform/GenericPlatformMisc.h"

#include <sstream>

namespace Wraith {
	void Guid::Serialize(YAML::Emitter& out) const {
		out << ToString(GuidFormat::DigitsWithHyphens);
	}

	void Guid::Deserialize(const YAML::Node& node) {
		if (node && node.IsScalar()) {
			if (!Parse(node.as<std::string>(), *this)) {
				Invalidate();
			}
		}
		else {
			Invalidate();
		}
	}

	std::string Guid::ToString(GuidFormat Format) const {
		std::ostringstream ss;
		auto toHex = [](uint32_t val, int width = 8, bool upper = true) -> std::string {
			std::ostringstream tmp;
			tmp << std::hex << std::setw(width) << std::setfill('0') << val;
			std::string s = tmp.str();
			if (upper) std::transform(s.begin(), s.end(), s.begin(), ::toupper);
			else std::transform(s.begin(), s.end(), s.begin(), ::tolower);
			return s;
		};

		auto writeBlocks = [&](bool upper = true) {
			ss << toHex(A, 8, upper) << "-";
			ss << toHex(B, 4, upper) << "-";
			ss << toHex(C, 4, upper) << "-";
			ss << toHex(D, 4, upper) << "-";
			ss << toHex(D, 12, upper); // last block 12 digits
		};

		switch (Format) {
			case GuidFormat::Digits:
				ss << toHex(A, 8) << toHex(B, 8) << toHex(C, 8) << toHex(D, 8);
				break;
			case GuidFormat::DigitsLower:
				ss << toHex(A, 8, false) << toHex(B, 8, false) << toHex(C, 8, false) << toHex(D, 8, false);
				break;
			case GuidFormat::DigitsWithHyphens:
				ss << toHex(A, 8) << "-" << toHex(B, 4) << "-" << toHex(C, 4) << "-" << toHex(D, 4) << "-" << toHex(D, 12);
				break;
			case GuidFormat::DigitsWithHyphensLower:
				ss << toHex(A, 8, false) << "-" << toHex(B, 4, false) << "-" << toHex(C, 4, false) << "-" << toHex(D, 4, false) << "-" << toHex(D, 12, false);
				break;
			case GuidFormat::DigitsWithHyphensInBraces:
				ss << "{" << toHex(A, 8) << "-" << toHex(B, 4) << "-" << toHex(C, 4) << "-" << toHex(D, 4) << "-" << toHex(D, 12) << "}";
				break;
			case GuidFormat::DigitsWithHyphensInParentheses:
				ss << "(" << toHex(A, 8) << "-" << toHex(B, 4) << "-" << toHex(C, 4) << "-" << toHex(D, 4) << "-" << toHex(D, 12) << ")";
				break;
			case GuidFormat::HexValuesInBraces:
				ss << "{0x" << toHex(A) << ",0x" << toHex(B) << ",0x" << toHex(C) << ",{0x"
					<< toHex(D, 2) << ",0x" << toHex(D, 2) << ",0x" << toHex(D, 2) << ",0x"
					<< toHex(D, 2) << ",0x" << toHex(D, 2) << ",0x" << toHex(D, 2) << ",0x"
					<< toHex(D, 2) << ",0x" << toHex(D, 2) << "}}";
				break;
			case GuidFormat::UniqueObjectGuid:
				ss << toHex(A, 8) << "-" << toHex(B, 8) << "-" << toHex(C, 8) << "-" << toHex(D, 8);
				break;
			case GuidFormat::Short:
			{
				std::string full = toHex(A, 8) + toHex(B, 8) + toHex(C, 8) + toHex(D, 8);
				std::string encoded;
				const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
				for (size_t i = 0; i < full.size(); i += 3) {
					uint32_t val = std::stoi(full.substr(i, 3), nullptr, 16);
					encoded += table[val % 64];
				}
				ss << encoded;
				break;
			}
			case GuidFormat::Base36Encoded:
			{
				uint64_t val = (static_cast<uint64_t>(A) << 32) | B;
				const char* alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
				std::string result;
				do {
					result.push_back(alphabet[val % 36]);
					val /= 36;
				} while (val > 0);
				std::reverse(result.begin(), result.end());
				ss << result;
				break;
			}
		}

		return ss.str();
	}

	uint64_t Guid::Hash() const {
		uint64_t hash = 14695981039346656037ull; // FNV offset basis
		hash ^= static_cast<uint64_t>(A); hash *= 1099511628211ull;
		hash ^= static_cast<uint64_t>(B); hash *= 1099511628211ull;
		hash ^= static_cast<uint64_t>(C); hash *= 1099511628211ull;
		hash ^= static_cast<uint64_t>(D); hash *= 1099511628211ull;
		return hash;
	}

	Guid Guid::NewGuid() {
		Guid Result(0, 0, 0, 0);
		Misc::CreateGuid(Result);
		return Result;
	}

	bool Guid::Parse(const std::string& GuidString, Guid& OutGuid) {
		std::string s = GuidString;

		// Strip the braces/parentheses
		if (!s.empty() && (s.front() == '{' || s.front() == '(')) {
			s = s.substr(1, s.size() - 2);
		}

		// Remove hyphens
		std::string compact;
		for (char c : s) {
			if (c != '-') compact.push_back(c);
		}

		if (compact.size() != 32) return false;

		try {
			OutGuid.A = std::stoul(compact.substr(0, 8), nullptr, 16);
			OutGuid.B = std::stoul(compact.substr(8, 8), nullptr, 16);
			OutGuid.C = std::stoul(compact.substr(16, 8), nullptr, 16);
			OutGuid.D = std::stoul(compact.substr(24, 8), nullptr, 16);
		}
		catch (...) {
			return false;
		}
		return true;
	}
}
