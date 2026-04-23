#pragma once

#include <array>
#include <span>

#include "../PICA/pica_vertex.hpp"
#include "../PICA/regs.hpp"

class GPU;

class SwRasterizer {
  public:
	explicit SwRasterizer(GPU& gpu) : gpu(gpu) {}

	void drawTriangles(
		u32 colorBufferLoc, PICA::ColorFmt colorFormat, u32 depthBufferLoc, PICA::DepthFmt depthFormat,
		std::array<u32, 2> fbSize, std::span<const PICA::Vertex> vertices
	);

  private:
	GPU& gpu;
};
