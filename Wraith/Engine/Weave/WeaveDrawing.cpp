#include "wpch.h"
#include "WeaveDrawing.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

namespace Wraith {
	void Drawing::DrawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color, ImU32 innerColor) {
		ImRect rect = ImRect(a, b);
		float rectX = rect.Min.x;
		float rectY = rect.Min.y;
		float rectW = rect.Max.x - rect.Min.x;
		float rectH = rect.Max.y - rect.Min.y;
		float rectCenterX = (rect.Min.x + rect.Max.x) * 0.5f;
		float rectCenterY = (rect.Min.y + rect.Max.y) * 0.5f;
		ImVec2 rectCenter = ImVec2(rectCenterX, rectCenterY);

		const float outlineScale = rectW / 24.0f;
		const int extraSegments = static_cast<int>(2 * outlineScale); // For the full circle

		if (type == IconType::Flow) {
			const float originScale = rectW / 24.0f;

			const float offsetX = 1.0f * originScale;
			const float offsetY = 0.0f * originScale;

			const float margin = (filled ? 2.0f : 2.0f) * originScale;
			const float rounding = 0.1f * originScale;
			const float tipRound = 0.7f; // Percentage of triangle edge

			const ImRect canvas = ImRect(
				rect.Min.x + margin + offsetX,
				rect.Min.y + margin + offsetY,
				rect.Max.x - margin + offsetX,
				rect.Max.y - margin + offsetY
			);
			const float canvasX = canvas.Min.x;
			const float canvasY = canvas.Min.y;
			const float canvasW = canvas.Max.x - canvas.Min.x;
			const float canvasH = canvas.Max.y - canvas.Min.y;

			const float left = canvasX + canvasW * 0.5f * 0.3f;
			const float right = canvasX + canvasW - canvasW * 0.5f * 0.3f;
			const float top = canvasY + canvasH * 0.5f * 0.2f;
			const float bottom = canvasY + canvasH - canvasH * 0.5f * 0.2f;
			const float centerY = (top + bottom) * 0.5f;

			const ImVec2 tipTop = ImVec2(canvasX + canvasW * 0.5f, top);
			const ImVec2 tipRight = ImVec2(right, centerY);
			const ImVec2 tipBottom = ImVec2(canvasX + canvasW * 0.5f, bottom);

			drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
			drawList->PathBezierCubicCurveTo(
				ImVec2(left, top),
				ImVec2(left, top),
				ImVec2(left, top) + ImVec2(rounding, 0)
			);
			drawList->PathLineTo(tipTop);
			drawList->PathLineTo(tipTop + (tipRight - tipTop) * tipRound);
			drawList->PathBezierCubicCurveTo(
				tipRight,
				tipRight,
				tipBottom + (tipRight - tipBottom) * tipRound
			);
			drawList->PathLineTo(tipBottom);
			drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
			drawList->PathBezierCubicCurveTo(
				ImVec2(left, bottom),
				ImVec2(left, bottom),
				ImVec2(left, bottom) - ImVec2(0, rounding)
			);

			if (!filled) {
				if (innerColor & 0xFF000000) drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

				drawList->PathStroke(color, true, 2.0f * outlineScale);
			}
			else {
				drawList->PathFillConvex(color);
			}
		}
		else {
			float triangleStart = rectCenterX + 0.32f * rectW;
			int rectOffset = -static_cast<int>(rectW * 0.25f * 0.25f);

			rect.Min.x += rectOffset;
			rect.Max.x += rectOffset;
			rectX += rectOffset;
			rectCenterX += rectOffset * 0.5f;
			rectCenter.x += rectOffset * 0.5f;

			if (type == IconType::Circle) {
				if (!filled) {
					const float radius = 0.5f * rectW / 2.0f - 0.5f;

					if (innerColor & 0xFF000000) drawList->AddCircleFilled(rectCenter, radius, innerColor, 12 + extraSegments);

					drawList->AddCircle(rectCenter, radius, color, 12 + extraSegments, 2.0f * outlineScale);
				}
				else {
					drawList->AddCircle(rectCenter, 0.5f * rectW / 2.0f, color, 12 + extraSegments);
				}
			}
			else if (type == IconType::Square) {
				const float radius = 0.5f * rectW / 2.0f;
				const ImVec2 p0 = rectCenter - ImVec2(radius, radius);
				const ImVec2 p1 = rectCenter + ImVec2(radius, radius);

				if (filled) {
					drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
				}
				else {
					if (innerColor & 0xFF000000) drawList->AddRectFilled(p0, p1, innerColor, 0, ImDrawFlags_RoundCornersAll);

					drawList->AddRect(p0, p1, color, 0, ImDrawFlags_RoundCornersAll, 2.0f * outlineScale);
				}
			}
			else if (type == IconType::Grid) {
				const float radius = 0.5f * rectW / 2.0f;
				const auto width = ceilf(radius / 3.0f);

				const ImVec2 baseTl = ImVec2(floorf(rectCenterX - width * 2.5f), floorf(rectCenterY - width * 2.5f));
				const ImVec2 baseBr = ImVec2(floorf(baseTl.x + width), floorf(baseTl.y + width));

				ImVec2 tl = baseTl;
				ImVec2 br = baseBr;
				for (int i = 0; i < 3; ++i) {
					tl.x = baseTl.x;
					br.x = baseBr.x;
					drawList->AddRectFilled(tl, br, color);

					tl.x += width * 2;
					br.x += width * 2;
					if (i != 1 || filled) drawList->AddRectFilled(tl, br, color);

					tl.x += width * 2;
					br.x += width * 2;
					drawList->AddRectFilled(tl, br, color);

					tl.y += width * 2;
					br.y += width * 2;
				}

				triangleStart = br.x + width + 1.0f / 24.0f * rectW;
			}

			// This isn't else if bc we want everything to run on the else
			if (type == IconType::RoundSquare) {
				const float radius = 0.5f * rectW / 2.0f;
				const float cornerRadius = radius * 0.5f;

				const ImVec2 p0 = rectCenter - ImVec2(radius, radius);
				const ImVec2 p1 = rectCenter + ImVec2(radius, radius);

				if (filled) {
					drawList->AddRectFilled(p0, p1, color, cornerRadius, ImDrawFlags_RoundCornersAll);
				}
				else {
					if (innerColor & 0xFF000000) drawList->AddRectFilled(p0, p1, innerColor, cornerRadius, ImDrawFlags_RoundCornersAll);

					drawList->AddRect(p0, p1, color, cornerRadius, ImDrawFlags_RoundCornersAll, 2.0f * outlineScale);
				}
			}
			else if (type == IconType::Diamond) {
				const float radius = 0.607f * rectW / 2.0f;
				const ImVec2 center = rectCenter;

				drawList->PathLineTo(center + ImVec2(0, -radius));
				drawList->PathLineTo(center + ImVec2(radius, 0));
				drawList->PathLineTo(center + ImVec2(0, radius));
				drawList->PathLineTo(center + ImVec2(-radius, 0));

				if (filled) {
					drawList->PathFillConvex(color);
				}
				else {
					if (innerColor & 0xFF000000) drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

					drawList->PathStroke(color, true, 2.0f * outlineScale);
				}
			}
			else {
				const float triangleTip = triangleStart + rectW * (0.45f - 0.32f);

				drawList->AddTriangleFilled(
					ImVec2(ceilf(triangleTip), rectY + rectH * 0.5f),
					ImVec2(triangleStart, rectCenterY + 0.15f * rectH),
					ImVec2(triangleStart, rectCenterY - 0.15f * rectH),
					color
				);
			}
		}
	}
}
