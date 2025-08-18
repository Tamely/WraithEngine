#pragma once

#include "Wraith/GenericPlatform/GenericPlatformMemory.h"

#include <algorithm>
#include <initializer_list>

enum { INDEX_NONE = -1 };

template<typename T, typename SizeType = size_t>
class Array {
private:
	T* Data;
	SizeType ArrayNum;		// Current number of elements
	SizeType ArrayMax;		// Current capacity

	static constexpr SizeType DefaultCapacity = 4;
public:
	// Type aliases for STL compatibility
	using value_type = T;
	using size_type = SizeType;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;

	// Iterator support
	using iterator = T*;
	using const_iterator = const T*;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	// Constructors
	Array() : Data(nullptr), ArrayNum(0), ArrayMax(0) {}

	explicit Array(SizeType InitialSize) 
		: Data(nullptr), ArrayNum(0), ArrayMax(0) {
		Reserve(InitialSize);
		ArrayNum = InitialSize;
		for (SizeType i = 0; i < ArrayNum; ++i) {
			new(Data + i) T();
		}
	}

	Array(SizeType InitialSize, const T& DefaultValue) 
		: Data(nullptr), ArrayNum(0), ArrayMax(0) {
		Reserve(InitialSize);
		ArrayNum = InitialSize;
		for (SizeType i = 0; i < ArrayNum; ++i) {
			new(Data + i) T(DefaultValue);
		}
	}

	Array(std::initializer_list<T> InitList) 
		: Data(nullptr), ArrayNum(0), ArrayMax(0) {
		Reserve(InitList.size());
		for (const auto& Item : InitList) {
			Add(Item);
		}
	}

	Array(const Array& Other) // Copy constructor
		: Data(nullptr), ArrayNum(0), ArrayMax(0) {
		*this = Other;
	} 

	Array(const Array&& Other) noexcept // Move constructor
		: Data(Other.Data), ArrayNum(Other.ArrayNum), ArrayMax(Other.ArrayMax) {
		Other.Data = nullptr;
		Other.ArrayNum = 0;
		Other.ArrayMax = 0;
	} 

	~Array() {
		Empty();
		if (Data) {
			Wraith::Memory::Free(Data);
		}
	}

	// Assignment operators
	Array& operator=(const Array& Other) {
		if (this != &Other) {
			Empty();
			Reserve(Other.ArrayNum);
			ArrayNum = Other.ArrayNum;
			for (SizeType i = 0; i < ArrayNum; ++i) {
				new(Data + i) T(Other.Data[i]);
			}
		}
		return *this;
	}
	Array& operator=(Array&& Other) noexcept {
		if (this != &Other) {
			Empty();
			if (Data) {
				Wraith::Memory::Free(Data);
			}

			Data = Other.Data;
			ArrayNum = Other.ArrayNum;
			ArrayMax = Other.ArrayMax;

			Other.Data = nullptr;
			Other.ArrayNum = 0;
			Other.ArrayMax = 0;
		}
		return *this;
	}

	// Element access
	T& operator[](SizeType Index) {
		return Data[Index];
	}
	const T& operator[](SizeType Index) const {
		return Data[Index];
	}

	T& At(SizeType Index) {
		W_CORE_ASSERT(Index < ArrayNum, "Index out of range");
		return Data[Index];
	}
	const T& At(SizeType Index) const {
		W_CORE_ASSERT(Index < ArrayNum, "Index out of range");
		return Data[Index];
	}

	T& Top() {
		return Data[ArrayNum - 1];
	}

	const T& Top() const {
		return Data[ArrayNum - 1];
	}

	T& Last(SizeType IndexFromEnd = 0) {
		return Data[ArrayNum - 1 - IndexFromEnd];
	}
	const T& Last(SizeType IndexFromEnd = 0) const {
		return Data[ArrayNum - 1 - IndexFromEnd];
	}

	// Size and capacity
	FORCEINLINE SizeType Num() const { return ArrayNum; }
	FORCEINLINE SizeType Size() const { return ArrayNum; }
	FORCEINLINE SizeType Max() const { return ArrayMax; }
	FORCEINLINE SizeType Capacity() const { return ArrayMax; }
	FORCEINLINE bool IsEmpty() const { return ArrayNum == 0; }

	// Modifiers
	void Reserve(SizeType NewCapacity) {
		if (NewCapacity > ArrayMax) {
			T* NewData = static_cast<T*>(Wraith::Memory::Malloc(NewCapacity * sizeof(T)));
			W_CORE_ASSERT(NewData);

			// Move existing elements
			for (SizeType i = 0; i < ArrayNum; ++i) {
				new(NewData + i) T(std::move(Data[i]));
				Data[i].~T();
			}

			if (Data) {
				Wraith::Memory::Free(Data);
			}

			Data = NewData;
			ArrayMax = NewCapacity;
		}
	}
	void Resize(SizeType NewSize) {
		if (NewSize > ArrayNum) {
			Reserve(NewSize);
			for (SizeType i = ArrayNum; i < NewSize; ++i) {
				new(Data + i) T();
			}
		}
		else if (NewSize < ArrayNum) {
			for (SizeType i = NewSize; i < ArrayNum; ++i) {
				Data[i].~T();
			}
		}
		ArrayNum = NewSize;
	}
	void Resize(SizeType NewSize, const T& DefaultValue) {
		if (NewSize > ArrayNum) {
			Reserve(NewSize);
			for (SizeType i = ArrayNum; i < NewSize; ++i) {
				new(Data + i) T(DefaultValue);
			}
		}
		else if (NewSize < ArrayNum) {
			for (SizeType i = NewSize; i < ArrayNum; ++i) {
				Data[i].~T();
			}
		}
		ArrayNum = NewSize;
	}

	SizeType Add(const T& Item) {
		if (ArrayNum >= ArrayMax) {
			SizeType NewCapacity = ArrayMax == 0 ? DefaultCapacity : ArrayMax * 2;
			Reserve(NewCapacity);
		}

		new(Data + ArrayNum) T(Item);
		return ArrayNum++;
	}
	SizeType Add(const T&& Item) {
		if (ArrayNum >= ArrayMax) {
			SizeType NewCapacity = ArrayMax == 0 ? DefaultCapacity : ArrayMax * 2;
			Reserve(NewCapacity);
		}

		new(Data + ArrayNum) T(std::move(Item));
		return ArrayNum++;
	}

	template<typename... Args>
	SizeType Emplace(Args&&... Arguments) {
		if (ArrayNum >= ArrayMax) {
			SizeType NewCapacity = ArrayMax == 0 ? DefaultCapacity : ArrayMax * 2;
			Reserve(NewCapacity);
		}

		new(Data + ArrayNum) T(std::forward<Args>(Arguments)...);
		return ArrayNum++;
	}

	void Insert(const T& Item, SizeType Index) {
		W_CORE_ASSERT(Index <= ArrayNum, "Index out of range");

		if (ArrayNum >= ArrayMax) {
			SizeType NewCapacity = ArrayMax == 0 ? DefaultCapacity : ArrayMax * 2;
			Reserve(NewCapacity);
		}

		// Move elements to make space
		for (SizeType i = ArrayNum; i > Index; --i) {
			new(Data + i) T(std::move(Data[i - 1]));
			Data[i - 1].~T();
		}

		new(Data + Index) T(Item);
		ArrayNum++;
	}

	void Pop() {
		if (ArrayNum > 0) {
			ArrayNum--;
			Data[ArrayNum].~T();
		}
	}

	bool RemoveAt(SizeType Index) {
		if (Index >= ArrayNum) return false;

		Data[Index].~T();

		// Shift elements down
		for (SizeType i = Index; i < ArrayNum - 1; ++i) {
			new(Data + i) T(std::move(Data[i + 1]));
			Data[i + 1].~T();
		}

		ArrayNum--;
		return true;
	}
	SizeType Remove(const T& Item) {
		SizeType NumRemoved = 0;
		for (SizeType i = 0; i < ArrayNum;) {
			if (Data[i] == Item) {
				RemoveAt(i);
				NumRemoved++;
			}
			else {
				i++;
			}
		}
		return NumRemoved;
	}

	void Empty() {
		for (SizeType i = 0; i < ArrayNum; ++i) {
			Data[i].~T();
		}
		ArrayNum = 0;
	}
	void Reset() {
		Empty();
		ArrayMax = 0;
		if (data) {
			Wraith::Memory::Free(Data);
			Data = nullptr;
		}
	}
	void Shrink() {
		if (ArrayNum < ArrayMax) {
			if (ArrayNum == 0) {
				Reset();
			}
			else {
				T* NewData = static_cast<T*>(Wraith::Memory::Malloc(ArrayNum * sizeof(T)));
				if (NewData) {
					for (SizeType i = 0; i < ArrayNum; ++i) {
						new(NewData + i) T(std::move(Data[i]));
						Data[i].~T();
					}

					Wraith::Memory::Free(Data);
					Data = NewData;
					ArrayMax = ArrayNum;
				}
			}
		}
	}

	// Search functions
	SizeType Find(const T& Item) const {
		for (SizeType i = 0; i < ArrayNum; ++i) {
			if (Data[i] == Item) {
				return i;
			}
		}
		return static_cast<SizeType>(INDEX_NONE);
	}
	bool Contains(const T& Item) const {
		return Find(Item) != static_cast<SizeType>(INDEX_NONE);
	}

	// Iterator support
	iterator begin() { return Data; }
	iterator end() { return Data + ArrayNum; }
	const_iterator begin() const { return Data; }
	const_iterator end() const { return Data + ArrayNum; }
	const_iterator cbegin() const { return Data; }
	const_iterator cend() const { return Data + ArrayNum; }

	// Reverse iterator support
	reverse_iterator rbegin() { return reverse_iterator(end()); }
	reverse_iterator rend() { return reverse_iterator(begin()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
	const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

	// Raw data access
	T* GetData() { return Data; }
	const T* GetData() const { return Data; }

	// Utility functions
	void Append(const Array& Other) {
		Reserve(ArrayNum + Other.ArrayNum);
		for (SizeType i = 0; i < Other.ArrayNum; ++i) {
			Add(Other.Data[i]);
		}
	}

	void Sort() {
		std::sort(begin(), end());
	}
	template<typename Predicate>
	void Sort(Predicate Pred) {
		std::sort(begin(), end(), Pred);
	}

	// Comparison operators
	bool operator==(const Array& Other) const {
		if (ArrayNum != Other.ArrayNum) return false;

		for (SizeType i = 0; i < ArrayNum; ++i) {
			if (Data[i] != Other.Data[i]) {
				return false;
			}
		}
		return true;
	}
	bool operator!=(const Array& Other) const {
		return !(*this == Other);
	}
};
