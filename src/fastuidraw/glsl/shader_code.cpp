/*!
 * \file shader_code.cpp
 * \brief file shader_code.cpp
 *
 * Copyright 2016 by Intel.
 *
 * Contact: kevin.rogovin@intel.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 * \author Kevin Rogovin <kevin.rogovin@intel.com>
 *
 */

#include <sstream>

#include <fastuidraw/util/math.hpp>
#include <fastuidraw/glsl/shader_code.hpp>
#include <fastuidraw/text/glyph_render_data_curve_pair.hpp>

namespace
{
   class Ending
  {
  public:
    explicit
    Ending(unsigned int N, enum fastuidraw::GlyphRenderDataCurvePair::geometry_packing pk):
      m_N(N),
      m_pk(pk)
    {}

    unsigned int m_N;
    enum fastuidraw::GlyphRenderDataCurvePair::geometry_packing m_pk;

  };

  std::ostream&
  operator<<(std::ostream &str, Ending obj)
  {
    int tempIndex, cc;
    const char *component[]=
      {
        "r",
        "g",
        "b",
        "a"
      };

    tempIndex = obj.m_pk / obj.m_N;
    cc = obj.m_pk - obj.m_N * tempIndex;
    assert(0<= cc && cc < 4);
    if(obj.m_N > 1)
      {
        str << tempIndex << "." << component[cc];
      }
    else
      {
        str << tempIndex;
      }
    return str;
  }

  class LoaderMacro
  {
  public:
    LoaderMacro(unsigned int alignment, const char *geometry_store_name);

    const char*
    value(void) const
    {
      return m_value.c_str();
    }
  private:
    std::string m_value;
  };
}

////////////////////////////////////////////////////
// LoaderMacro methods
LoaderMacro::
LoaderMacro(unsigned int alignment, const char *geometry_store_fetch)
{
  /* we are defining a macro, so on end of lines
     in code we need to add \\
   */
  std::ostringstream str;
  const char *texelFetchExt[4] =
    {
      "r",
      "rg",
      "rgb",
      "rgba"
    };
  const char *tempType[4] =
    {
      "float",
      "vec2",
      "vec3",
      "vec4"
    };

  str << "\n#define FASTUIDRAW_LOAD_CURVE_GEOMETRY(geometry_offset, texel_value) { \\\n"
      << "\t" << tempType[alignment - 1] << " ";

  for(unsigned int c = 0, j = 0; c < fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack; c += alignment, ++j)
    {
      if(c != 0)
        {
          str << ", ";
        }
      str << "temp" << j;
    }
  str << ";\\\n"
      << "\tint start_offset;\\\n"
      << "\tstart_offset = int(geometry_offset) + ( int(texel_value) - 2 ) * int("
      << fastuidraw::round_up_to_multiple(fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack, alignment) / alignment
      <<");\\\n";

  for(unsigned int c = 0, j = 0; c < fastuidraw::GlyphRenderDataCurvePair::number_elements_to_pack; c += alignment, ++j)
    {
      str << "\ttemp" << j << " = " << geometry_store_fetch << "(start_offset + "
          << j << ")." << texelFetchExt[alignment - 1] << ";\\\n";
    }

  /* this is scary, the shader code will have the macros/whatever defined for us so that
     that assignment from temp "just works".
   */
#define EXTRACT_STREAM(X) str << "\t"#X << " = temp" << Ending(alignment, fastuidraw::GlyphRenderDataCurvePair::pack_offset_##X) << ";\\\n"


  EXTRACT_STREAM(p_x);
  EXTRACT_STREAM(p_y);
  EXTRACT_STREAM(zeta);
  EXTRACT_STREAM(combine_rule);

  EXTRACT_STREAM(curve0_m0);
  EXTRACT_STREAM(curve0_m1);
  EXTRACT_STREAM(curve0_q_x);
  EXTRACT_STREAM(curve0_q_y);
  EXTRACT_STREAM(curve0_quad_coeff);

  EXTRACT_STREAM(curve1_m0);
  EXTRACT_STREAM(curve1_m1);
  EXTRACT_STREAM(curve1_q_x);
  EXTRACT_STREAM(curve1_q_y);
  EXTRACT_STREAM(curve1_quad_coeff);

  str << "\\\n}\n";

  m_value = str.str();
}

/////////////////////////////////
// fastuidraw::glsl::code methods
fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
curvepair_compute_pseudo_distance(unsigned int alignment,
                                  const char *function_name,
                                  const char *geometry_store_fetch,
                                  bool derivative_function)
{
  ShaderSource return_value;

  /* aww, this sucks; we can choose one of 4 options:
       1. dedicated shader for all variations.
       2. a loader function (but we need to prefix its name)
          along with the global values
       3. break function into sections:
          - function declaration and local variables
          - loader code
          - computation and closing code
       4. Define the loading macro in C++ and make sure the
          variable names used match between this C++ code
          and the GLSL shader code
     We choose option 4; which sucks for reading the shader code.
   */

  return_value
    .add_macro("FASTUIDRAW_CURVEPAIR_COMPUTE_NAME", function_name)
    .add_source(LoaderMacro(alignment, geometry_store_fetch).value(),
                ShaderSource::from_string);

  if(derivative_function)
    {
      return_value
        .add_source("fastuidraw_curvepair_glyph_derivative.frag.glsl.resource_string",
                    ShaderSource::from_resource);
    }
  else
    {
      return_value
        .add_source("fastuidraw_curvepair_glyph.frag.glsl.resource_string",
                    ShaderSource::from_resource);
    }

  return_value
    .add_source("#undef FASTUIDRAW_LOAD_CURVE_GEOMETRY\n", glsl::ShaderSource::from_string)
    .remove_macro("FASTUIDRAW_CURVEPAIR_COMPUTE_NAME");

  return return_value;

}

fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
image_atlas_compute_coord(const char *function_name,
                          const char *index_texture,
                          unsigned int index_tile_size,
                          unsigned int color_tile_size)
{
  ShaderSource return_value;

  return_value
    .add_macro("FASTUIDRAW_INDEX_TILE_SIZE", index_tile_size)
    .add_macro("FASTUIDRAW_COLOR_TILE_SIZE", color_tile_size)
    .add_macro("FASTUIDRAW_INDEX_ATLAS", index_texture)
    .add_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD", function_name)
    .add_source("fastuidraw_atlas_image_fetch.glsl.resource_string", glsl::ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_INDEX_TILE_SIZE")
    .remove_macro("FASTUIDRAW_COLOR_TILE_SIZE")
    .remove_macro("FASTUIDRAW_INDEX_ATLAS")
    .remove_macro("FASTUIDRAW_ATLAS_COMPUTE_COORD");

  return return_value;
}


fastuidraw::glsl::ShaderSource
fastuidraw::glsl::code::
dashed_stroking_compute(const char *function_name,
                        unsigned int data_alignment)
{
  ShaderSource return_value;
  const char *src[] =
    {
      "fastuidraw_compute_dash_stroke_alignment_1.glsl.resource_string",
      "fastuidraw_compute_dash_stroke_alignment_2.glsl.resource_string",
      "fastuidraw_compute_dash_stroke_alignment_3.glsl.resource_string",
      "fastuidraw_compute_dash_stroke_alignment_4.glsl.resource_string"
    };

  data_alignment = t_max(1u, t_min(data_alignment, 4u));

  return_value
    .add_macro("FASTUIDRAW_COMPUTE_DASH_STROKE", function_name)
    .add_source(src[data_alignment - 1], glsl::ShaderSource::from_resource)
    .remove_macro("FASTUIDRAW_COMPUTE_DASH_STROKE");
  return return_value;
}
