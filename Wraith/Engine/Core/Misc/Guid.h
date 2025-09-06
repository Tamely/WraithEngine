#pragma once

#include "Core/Log.h"
#include "Core/CoreBasic.h"

#include <cstdint>
#include <string>

#include <yaml-cpp/yaml.h>

namespace Wraith {
	enum class GuidFormat {
		Digits,
		DigitsLower,
		DigitsWithHyphens,
		DigitsWithHyphensLower,
		DigitsWithHyphensInBraces,
		DigitsWithHyphensInParentheses,
		HexValuesInBraces,
		UniqueObjectGuid,
		Short,
		Base36Encoded
	};

	struct Guid {
	public:
		constexpr Guid()
			: A(0)
			, B(0)
			, C(0)
			, D(0) {}

		explicit constexpr Guid(uint32_t InA, uint32_t InB, uint32_t InC, uint32_t InD)
			: A(InA), B(InB), C(InC), D(InD) {}

		explicit Guid(const std::string& InGuidStr) {
			if (!Parse(InGuidStr, *this)) {
				Invalidate();
			}
		}
	public:
		friend bool operator==(const Guid& x, const Guid& y) {
			return ((x.A ^ y.A) | (x.B ^ y.B) | (x.C ^ y.C) | (x.D ^ y.D)) == 0;
		}

		friend bool operator!=(const Guid& x, const Guid& y) {
			return ((x.A ^ y.A) | (x.B ^ y.B) | (x.C ^ y.C) | (x.D ^ y.D)) != 0;
		}

		friend bool operator<(const Guid& x, const Guid& y) {
			return	((x.A < y.A) ? true : ((x.A > y.A) ? false :
					((x.B < y.B) ? true : ((x.B > y.B) ? false :
					((x.C < y.C) ? true : ((x.C > y.C) ? false :
					((x.D < y.D) ? true : ((x.D > y.D) ? false : false))))))));
		}

		uint32_t& operator[](int32_t index) {
			W_CORE_VERIFY(index >= 0, "GUID operator[] index out of range");
			W_CORE_VERIFY(index < 4, "GUID operator[] index out of range");

			switch (index) {
				case 0: return A;
				case 1: return B;
				case 2: return C;
				case 3: return D;
			}
			return A;
		}

		const uint32_t& operator[](int32_t index) const {
			W_CORE_VERIFY(index >= 0, "GUID operator[] index out of range");
			W_CORE_VERIFY(index < 4, "GUID operator[] index out of range");

			switch (index) {
				case 0: return A;
				case 1: return B;
				case 2: return C;
				case 3: return D;
			}
			return A;
		}

		void Serialize(YAML::Emitter& out) const;
		void Deserialize(const YAML::Node& node);

		void Invalidate() {
			A = B = C = D = 0;
		}

		bool IsValid() const {
			return ((A | B | C | D) != 0);
		}

		std::string ToString(GuidFormat Format = GuidFormat::Digits) const;
		uint64_t Hash() const;
	public:
		uint32_t A, B, C, D;
	public:
		static Guid NewGuid();
		static bool Parse(const std::string& GuidString, Guid& OutGuid);
	};
}

namespace std {
	template<>
	struct hash<Wraith::Guid> {
		std::size_t operator()(const Wraith::Guid& guid) const {
			return guid.Hash();
		}
	};
}
