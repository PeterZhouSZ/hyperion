#ifndef EOS_STEREO_DIFFUSE_CORRELATION_H
#define EOS_STEREO_DIFFUSE_CORRELATION_H
//------------------------------------------------------------------------------
// Copyright 2009 Tom Haines

/// \file diffuse_correlation.h
/// Provides advanced correlation capabilities, that weight pixels via diffusion
/// and use colour ranges rather than points.

#include "eos/types.h"
#include "eos/time/progress.h"
#include "eos/bs/luv_range.h"


namespace eos
{
 namespace stereo
 {
//------------------------------------------------------------------------------
/// Diffusion weight object - stores the weights associated with going in each
/// direction from each pixel in an image. Has a method to calculate from a
/// LuvRangeImage using a LuvRangeDist.
class EOS_CLASS DiffusionWeight
{
 public:
  /// &nbsp;
   DiffusionWeight();
  
  /// &nbsp;
   ~DiffusionWeight();


  /// Fills in the object from a LuvRangeImage and a LuvRangeDist. It takes the
  /// negative exponential of the distances to get relative weightings, and 
  /// makes an effort for stability. It supports a distance multiplier before 
  /// it does this and respects masks by not sending any weight that way, or off
  /// the image.
   void Create(const bs::LuvRangeImage & img, const bs::LuvRangeDist & dist, real32 distMult = 1.0, time::Progress * prog = null<time::Progress*>());


  /// Returns the weight for a given pixel for a given direction. Note that this
  /// weight will be normalised between the 4 directions, which have the typical
  /// 0=+ve x, 1=+ve y,2=-ve x,3=-ve y coding, unless the pixel is invalid in 
  /// which case they will all be zero.
   real32 Get(nat32 x,nat32 y,nat32 dir) const {return data.Get(x,y).dir[dir];}


  /// &nbsp;
   inline cstrconst TypeString() const {return "eos::stereo::DiffusionWeight";}


 private:
  struct Weight
  {
   real32 dir[4];
  };
  
  ds::Array2D<Weight> data;
};

//------------------------------------------------------------------------------
/// Given a LuvRangeImage and a scanline number this calculates a slice of 
/// diffusion scores, for a given number of steps.
/// Clever enough to cache storage between runs as long as the image
/// width and step count don't change.
/// Once done each pixel in the scanline will have a normalised set of weights 
/// for surrounding pixels within the given walking distance. Note that this is
/// never going to be that fast. It will always give values of zero to masked or
/// out of bound values.
class EOS_CLASS RangeDiffusionSlice
{
 public:
  /// &nbsp;
   RangeDiffusionSlice();

  /// &nbsp;
   ~RangeDiffusionSlice();
   
   
  /// Creates the data for a diffusion slice - you give it an image, the 
  /// y-coordinate of the slice to calculate, how many steps to walk and a 
  /// diffusion weight object and it constructs the slices diffuson masks, one for
  /// each pixel in the slice.
  /// Will not walk off edges of the image, or into masked off areas.
  /// For weighting from the distances takes the negative exponential of
  /// distance. (And offsets first, for stability.)
   void Create(nat32 y, nat32 steps, const bs::LuvRangeImage & img, const DiffusionWeight & dw, time::Progress * prog = null<time::Progress*>());


  /// Returns the width of the slice.
   nat32 Width() const;
   
  /// Returns the y coordinate associated with the slice.
   nat32 Y() const {return y;}
   
  /// Returns the number of steps of the slice.
   nat32 Steps() const;
   
  /// Given a x-coordinate and a (u,v) window coordinate this returns the weight
  /// - out of range values will return 0.0, defined by abs(u) + abs(v) > steps.
   real32 Get(nat32 x,int32 u,int32 v) const;


  /// &nbsp;
   inline cstrconst TypeString() const {return "eos::stereo::RangeDiffusionSlice";} 


 private:
  // Storage for state...
   nat32 steps;
   nat32 y;
   
   ds::Array2D<real32> data; // Stores the x coordinate in x and the y coordinate is a linearlisation of the diffusion values. Will not have anything for masked entries.
   ds::Array2D<nat32> offset; // Index from (u+steps,v+steps) to the above linearisation. Only valid when abs(u) + abs(v) <= steps.
};

//------------------------------------------------------------------------------
/// This is given a pair of bs::LuvRangeImage's and RangeDiffusionSlice's,
/// it then calculates the correlation between pixels in the two slices.
/// It also makes use of a LuvRangeDist to calculate the difference between pixels.
/// Simply takes the distances weighted by the diffusion weights, added for the
/// two pixels in question. Due to the adding the result is divided by two when
/// done, the output is then a distance metric.
/// A distance cap is provided - distances are capped at this value, to
/// handle outliers. This is also the value used if either pixel is outside the
/// image or masked.
class EOS_CLASS DiffuseCorrelation
{
 public:
  /// &nbsp;
   DiffuseCorrelation();

  /// &nbsp;
   ~DiffuseCorrelation();


  /// Fills in the valid details - note that all passed in objects must survive
  /// the lifetime of this object.
   void Setup(const bs::LuvRangeDist & dist, real32 distCap, const bs::LuvRangeImage & img1, const RangeDiffusionSlice & dif1, const bs::LuvRangeImage & img2, const RangeDiffusionSlice & dif2);
   
  /// Returns the width of image 1.
   nat32 Width1() const;

  /// Returns the width of image 1.
   nat32 Width2() const;
   
  /// Given two x coordinates this returns their matching cost - note that this
  /// does the correlation and is a slow method call.
   real32 Cost(nat32 x1,nat32 x2) const;
   
  /// Returns the distance cap used.
   real32 DistanceCap() const;


  /// &nbsp;
   inline cstrconst TypeString() const {return "eos::stereo::DiffuseCorrelation";}


 private:
  const bs::LuvRangeDist * dist;
  real32 distCap;
  
  const bs::LuvRangeImage * img1;
  const RangeDiffusionSlice * dif1;
  const bs::LuvRangeImage * img2;
  const RangeDiffusionSlice * dif2;
};

//------------------------------------------------------------------------------
/// This calculates correlation scores for an image pair, using DiffuseCorrelation,
/// the ultimate result is a list of best matches, i.e. distance minimas for
/// each pixel in both images; it is symmetric in its output.
/// Each match will also include its score and the scores of adjacent pixels, so
/// the positions can be refined beyond discrete coordinates.
class EOS_CLASS DiffusionCorrelationImage
{
 public:
  /// &nbsp;
   DiffusionCorrelationImage();

  /// &nbsp;
   ~DiffusionCorrelationImage();


  /// Sets the distance measure and the left and right pyramids.
  /// The pyramids must have the same height and be constructed with the same
  /// settings. Halfing the width must always be on.
  /// Also sets the distance multiplier - this is only used for the diffusion
  /// when converting from distance to weight, and is not used for the 
  /// correlation.
   void Set(const bs::LuvRangeDist & dist, real32 distMult, const bs::LuvRangePyramid & left, const bs::LuvRangePyramid & right);
   
  /// Sets various parameters - the maximum number of maximas to store for each
  /// pixel and the distance cap to use at the base level, the multiplier 
  /// for it to adjust the distance cap for each higher level, and the multiplier
  /// to get the threshold for prunning output at each hierachy level.
  /// range is how many pixels either side of a maxima to store the correlation
  /// scores for - don't set it too high, range is the diffusion range to use.
   void Set(nat32 maximaLimit = 8, real32 baseDistCap = 1.0, real32 distCapMult = 2.0, real32 distCapThreshold = 0.5, nat32 range = 2, nat32 steps = 5);


  /// Runs the algorithm.
   void Run(time::Progress * prog = null<time::Progress*>());
   
  
  /// Returns the range that te algorithm was run with.
   int32 Range() const;


  /// Extracts how many maxima exist for a given left pixel.
  /// Can return 0 if it just doesn't know.
   nat32 CountLeft(nat32 x,nat32 y) const;
   
  /// Extracts the right image x for a given maxima i, for the given pixel in the left image.
  /// Noet that the maximas will be ordered from best scoring to worst scoring.
   nat32 DisparityLeft(nat32 x,nat32 y,nat32 i) const;
   
  /// Extracts the correlation distance score for the given maxima of the given
  /// pixel in the left image at the given offset.
  /// The offset must be in [-range,range]
   real32 ScoreLeft(nat32 x,nat32 y,nat32 i,int32 offset) const;
  

  /// Extracts how many maxima exist for a given right pixel.
  /// Can return 0 if it just doesn't know.
   nat32 CountRight(nat32 x,nat32 y) const;
   
  /// Extracts the right image x for a given maxima i, for the given pixel in the right image.
  /// Noet that the maximas will be ordered from best scoring to worst scoring.
   nat32 DisparityRight(nat32 x,nat32 y,nat32 i) const;
   
  /// Extracts the correlation distance score for the given maxima of the given
  /// pixel in the right image at the given offset.
  /// The offset must be in [-range,range]
   real32 ScoreRight(nat32 x,nat32 y,nat32 i,int32 offset) const;


  /// &nbsp;
   inline cstrconst TypeString() const {return "eos::stereo::DiffusionCorrelationImage";}


 private:
  // Inputs...
   const bs::LuvRangeDist * dist;
   const bs::LuvRangePyramid * left;
   const bs::LuvRangePyramid * right;
  
  // Parameters...
   real32 distMult;  
   nat32 maximaLimit;
   real32 baseDistCap;
   real32 distCapMult;
   real32 distCapThreshold;
   nat32 range;
   nat32 steps;

  // Outputs...
   ds::Array<nat32> offsetLeft; // Indexed by y*width + x - gives you the offset into dispLeft to ge thte data for a pixel, and if multiplier by (range*2+1) the offset into scoreLeft. Has on e xtra value so you can determine size by subtracting from the incrimented index.
   ds::Array<nat32> dispLeft;
   ds::Array<real32> scoreLeft;
   
   ds::Array<nat32> offsetRight;
   ds::Array<nat32> dispRight;
   ds::Array<real32> scoreRight;
   
  // Runtime...
   struct Match
   {
    int32 y;
    int32 xLeft;
    int32 xRight;
    real32 score;
    
    bit operator < (const Match & rhs) const
    {
     if (y!=rhs.y) return y<rhs.y;
     if (xLeft!=rhs.xLeft) return xLeft<rhs.xLeft;
     return xRight<rhs.xRight;
    }
   };
};

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
 };
};
#endif