#pragma once

#include <memory>

namespace Wraith {
	struct UIDragDropPayload {
		// Members
		void*			Data;               // Data
		int             DataSize;           // Data size

		// [Internal]
		void*			SourceId;           // Source item id
		void*			SourceParentId;     // Source parent id (if available)
		int             DataFrameCount;     // Data timestamp
		char            DataType[32 + 1];   // Data type tag (short user-supplied string, 32 characters max)
		bool            Preview;            // Set when AcceptDragDropPayload() was called and mouse has been hovering the target item (nb: handle overlapping drag targets)
		bool            Delivery;           // Set when AcceptDragDropPayload() was called and mouse button is released over the target item.

		UIDragDropPayload() { Clear(); }
		void Clear() { SourceId = SourceParentId = 0; Data = NULL; DataSize = 0; memset(DataType, 0, sizeof(DataType)); DataFrameCount = -1; Preview = Delivery = false; }
		bool IsDataType(const char* type) const { return DataFrameCount != -1 && strcmp(type, DataType) == 0; }
		bool IsPreview() const { return Preview; }
		bool IsDelivery() const { return Delivery; }
	};
}
