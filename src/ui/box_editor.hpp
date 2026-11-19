#pragma once

#include "geometry/box_model.hpp"

#include <string>

class BRepPreview;

class BoxEditor
{
  public:
    explicit BoxEditor(const BoxDimensions &dimensions) : draft_(dimensions)
    {
    }
    void draw(BRepPreview &preview);

  private:
    void draw_history(BRepPreview &preview);
    BoxDimensions draft_;
    std::string error_;
};
