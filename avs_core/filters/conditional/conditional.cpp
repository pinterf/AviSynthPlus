
// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#include "conditional.h"
#include "../../core/parser/scriptparser.h"
#include "conditional_reader.h"
#include <cmath>
#include <avs/win.h>
#include <avs/minmax.h>
#include "../../core/internal.h"

extern const AVSFunction Conditional_filters[] = {
  {  "ConditionalSelect", BUILTIN_FUNC_PREFIX, "csc+[show]b", ConditionalSelect::Create },
  {  "ConditionalFilter", BUILTIN_FUNC_PREFIX, "cccsss[show]b", ConditionalFilter::Create, (void *)0 },
  // easy syntax from GConditionalFilter, args3 and 4 to "=" and "true":
  {  "ConditionalFilter", BUILTIN_FUNC_PREFIX, "cccs[show]b", ConditionalFilter::Create, (void *)1 },
  {  "ScriptClip",        BUILTIN_FUNC_PREFIX, "cs[show]b[after_frame]b", ScriptClip::Create },
  {  "ConditionalReader", BUILTIN_FUNC_PREFIX, "css[show]b", ConditionalReader::Create },
  {  "FrameEvaluate",     BUILTIN_FUNC_PREFIX, "cs[show]b[after_frame]b", ScriptClip::Create_eval },
  {  "WriteFile",         BUILTIN_FUNC_PREFIX, "c[filename]ss+[append]b[flush]b", Write::Create },
  {  "WriteFileIf",       BUILTIN_FUNC_PREFIX, "c[filename]ss+[append]b[flush]b", Write::Create_If },
  {  "WriteFileStart",    BUILTIN_FUNC_PREFIX, "c[filename]ss+[append]b", Write::Create_Start },
  {  "WriteFileEnd",      BUILTIN_FUNC_PREFIX, "c[filename]ss+[append]b", Write::Create_End },
  { 0 }
};

#define W_DIVISOR 5  // Width divisor for onscreen messages


/********************************
 * Conditional Select
 *
 * Returns each one frame from N sources
 * based on an integer evaluator.
 ********************************/

ConditionalSelect::ConditionalSelect(PClip _child, const char _expression[],
                                     int _num_args, PClip *_child_array,
                                     bool _show, IScriptEnvironment* env) :
  GenericVideoFilter(_child), expression(_expression),
  num_args(_num_args), child_array(_child_array), show(_show) {
    
  for (int i=0; i<num_args; i++) {
    const VideoInfo& vin = child_array[i]->GetVideoInfo();

    if (vi.height != vin.height)
      env->ThrowError("ConditionalSelect: The sources must all have the same height!");

    if (vi.width != vin.width)
      env->ThrowError("ConditionalSelect: The sources must all have the same width!");

    if (!vi.IsSameColorspace(vin))
      env->ThrowError("ConditionalSelect: The sources must all be the same colorspace!");

    if (vi.num_frames < vin.num_frames) // Max of all clips
      vi.num_frames = vin.num_frames;
  }
}


ConditionalSelect::~ConditionalSelect() {
  delete[] child_array;
}

int __stdcall ConditionalSelect::SetCacheHints(int cachehints, int frame_range)
{
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

PVideoFrame __stdcall ConditionalSelect::GetFrame(int n, IScriptEnvironment* env) {

  AVSValue prev_last = env->GetVarDef("last");  // Store previous last
  env->SetVar("last", (AVSValue)child);      // Set implicit last
  AVSValue result;

  try {
    ScriptParser parser(env, expression, "[Conditional Select, Expression]", n);
    PExpression exp = parser.Parse();
    result = exp->Evaluate(env);

    if (!result.IsInt())
      env->ThrowError("Conditional Select: Expression must return an integer!");
  }
  catch (const AvisynthError &error) {    
    env->SetVar("last", prev_last);                   // Restore implicit last

    const int num_frames = child->GetVideoInfo().num_frames;
    PVideoFrame dst = child->GetFrame(min(num_frames-1, n), env);

    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi, error.msg, vi.width/W_DIVISOR, 0xa0a0a0, 0, 0);

    return dst;
  }

  env->SetVar("last", prev_last);                   // Restore implicit last

  const int i = result.AsInt();
  
  PVideoFrame dst;

  if (i < 0 || i >= num_args) {
    const int num_frames = child->GetVideoInfo().num_frames;
    dst = child->GetFrame(min(num_frames-1, n), env);
  }
  else {
    const int num_frames = child_array[i]->GetVideoInfo().num_frames;
    dst = child_array[i]->GetFrame(min(num_frames-1, n), env);
  }

  if (show) {
    char text[32];

    _snprintf(text, sizeof(text)-1, "Expression Result:%i\n", result.AsInt());
    text[sizeof(text)-1] = '\0';

    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi, text, vi.width/4, 0xa0a0a0, 0, 0);
  }

  return dst;
}


AVSValue __cdecl ConditionalSelect::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  int num_args = 0;
  PClip* child_array = 0;

  if (!args[1].AsString(0))
    env->ThrowError("Conditional Select: expression missing!");

  if (args[2].IsArray()) {
    num_args = args[2].ArraySize();
    child_array = new PClip[num_args];

    for (int i = 0; i < num_args; ++i) // Copy clips
      child_array[i] = args[2][i].AsClip();
  }
  else if (args[2].IsClip()) { // Make easy to call with trivial 1 clip
    num_args = 1;
    child_array = new PClip[1];

    child_array[0] = args[2].AsClip();
  }
  else {
    env->ThrowError("Conditional Select: clip array not recognized!");
  }

  return new ConditionalSelect(args[0].AsClip(), args[1].AsString(), num_args, child_array, args[3].AsBool(false), env);
}


/********************************
 * Conditional filter
 *
 * Returns each one frame from two sources,
 * based on an evaluator.
 ********************************/

ConditionalFilter::ConditionalFilter(PClip _child, PClip _source1, PClip _source2,
                                     AVSValue  _condition1, AVSValue  _evaluator, AVSValue  _condition2,
                                     bool _show, IScriptEnvironment* env) :
  GenericVideoFilter(_child), source1(_source1), source2(_source2),
  eval1(_condition1), eval2(_condition2), show(_show) {
    
    evaluator = NONE;

    if (lstrcmpi(_evaluator.AsString(), "equals") == 0 ||
        lstrcmpi(_evaluator.AsString(), "=") == 0 ||
        lstrcmpi(_evaluator.AsString(), "==") == 0)
      evaluator = EQUALS;
    if (lstrcmpi(_evaluator.AsString(), "greaterthan") == 0 || lstrcmpi(_evaluator.AsString(), ">") == 0)
      evaluator = GREATERTHAN;
    if (lstrcmpi(_evaluator.AsString(), "lessthan") == 0 || lstrcmpi(_evaluator.AsString(), "<") == 0)
      evaluator = LESSTHAN;

    if (evaluator == NONE)
      env->ThrowError("ConditionalFilter: Evaluator could not be recognized!");

    VideoInfo vi1 = source1->GetVideoInfo();
    VideoInfo vi2 = source2->GetVideoInfo();

    if (vi1.height != vi2.height)
      env->ThrowError("ConditionalFilter: The two sources must have the same height!");
    if (vi1.width != vi2.width)
      env->ThrowError("ConditionalFilter: The two sources must have the same width!");
    if (!vi1.IsSameColorspace(vi2))
      env->ThrowError("ConditionalFilter: The two sources must be the same colorspace!");

    vi.height = vi1.height;
    vi.width = vi1.width;
    vi.pixel_type = vi1.pixel_type;
    vi.num_frames = max(vi1.num_frames,vi2.num_frames);
    vi.num_audio_samples = vi1.num_audio_samples;
    vi.audio_samples_per_second = vi1.audio_samples_per_second;
    vi.image_type = vi1.image_type;
    vi.fps_denominator = vi1.fps_denominator;
    vi.fps_numerator = vi1.fps_numerator;
    vi.nchannels = vi1.nchannels;
    vi.sample_type = vi1.sample_type;
  }

const char* const t_TRUE="TRUE"; 
const char* const t_FALSE="FALSE";

int __stdcall ConditionalFilter::SetCacheHints(int cachehints, int frame_range)
{
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

PVideoFrame __stdcall ConditionalFilter::GetFrame(int n, IScriptEnvironment* env) {

  VideoInfo vi1 = source1->GetVideoInfo();
  VideoInfo vi2 = source2->GetVideoInfo();

  AVSValue prev_last = env->GetVarDef("last");  // Store previous last
  env->SetVar("last",(AVSValue)child);       // Set implicit last

  AVSValue e1_result;
  AVSValue e2_result;
  try {
    ScriptParser parser(env, eval1.AsString(), "[Conditional Filter, Expresion 1]", n);
    PExpression exp = parser.Parse();
    e1_result = exp->Evaluate(env);

    ScriptParser parser2(env, eval2.AsString(), "[Conditional Filter, Expression 2]", n);
    exp = parser2.Parse();
    e2_result = exp->Evaluate(env);
  } catch (const AvisynthError &error) {    
    const char* error_msg = error.msg;  

    PVideoFrame dst = source1->GetFrame(n,env);
    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi1, error_msg, vi.width/W_DIVISOR, 0xa0a0a0, 0, 0);
    env->SetVar("last",prev_last);       // Restore implicit last
    return dst;
  }

  env->SetVar("last",prev_last);       // Restore implicit last

  bool test_int=false;
  bool test_string=false;

  int e1 = 0;
  int e2 = 0;
  double f1 = 0.0;
  double f2 = 0.0;
  try {
    if (e1_result.IsString()) {
      if (!e2_result.IsString())
        env->ThrowError("Conditional filter: Second expression did not return a string, as in first string expression.");
      test_string = true;
      test_int = true;
      e1 = lstrcmp(e1_result.AsString(), e2_result.AsString());
      e2 = 0;

    } else if (e1_result.IsBool()) {
      if (!(e2_result.IsInt() || e2_result.IsBool()))
        env->ThrowError("Conditional filter: Second expression did not return an integer or bool, as in first bool expression.");
      test_int = true;
      e1 = e1_result.AsBool();
      e2 = e2_result.IsInt() ? e2_result.AsInt() : e2_result.AsBool();

    } else if (e1_result.IsInt()) {
      if (e2_result.IsInt() || e2_result.IsBool()) {
        test_int = true;
        e1 = e1_result.AsInt();
        e2 = e2_result.IsInt() ? e2_result.AsInt() : e2_result.AsBool();
      } else if (e2_result.IsFloat()) {
        f1 = (float)e1_result.AsFloat();
        f2 = (float)e2_result.AsFloat();
      } else
        env->ThrowError("Conditional filter: Second expression did not return a float, integer or bool, as in first integer expression.");

    } else if (e1_result.IsFloat()) {
      f1 = (float)e1_result.AsFloat();
      if (!e2_result.IsFloat()) 
        env->ThrowError("Conditional filter: Second expression did not return a float or an integer, as in first float expression.");
      f2 = (float)e2_result.AsFloat();
    } else {
      env->ThrowError("ConditionalFilter: First expression did not return an integer, bool or float!");
    }
  } catch (const AvisynthError &error) {    
    const char* error_msg = error.msg;  

    PVideoFrame dst = source1->GetFrame(n,env);
    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi1, error_msg, vi.width/W_DIVISOR, 0xa0a0a0, 0, 0);
    return dst;
  }


  bool state = false;

  if (test_int) { // String and Int compare
    if (evaluator&EQUALS) 
      if (e1 == e2) state = true;

    if (evaluator&GREATERTHAN) 
      if (e1 > e2) state = true;

    if (evaluator&LESSTHAN) 
      if (e1 < e2) state = true;

  } else {  // Float compare
    if (evaluator&EQUALS) 
      if (fabs(f1-f2)<0.000001) state = true;   // Exact equal will sometimes be rounded to wrong values.

    if (evaluator&GREATERTHAN) 
      if (f1 > f2) state = true;

    if (evaluator&LESSTHAN) 
      if (f1 < f2) state = true;    
  }

  if (show) {
      char text[400];
      if (test_string) {
        _snprintf(text, sizeof(text)-1,
          "Left side Conditional Result:%.40s\n"
          "Right side Conditional Result:%.40s\n"
          "Evaluate result: %s\n",
          e1_result.AsString(), e2_result.AsString(), (state) ? t_TRUE : t_FALSE
        );
      } else if (test_int) {
        _snprintf(text, sizeof(text)-1,
          "Left side Conditional Result:%i\n"
          "Right side Conditional Result:%i\n"
          "Evaluate result: %s\n",
          e1, e2, (state) ? t_TRUE : t_FALSE
        );
      } else {
        _snprintf(text, sizeof(text)-1,
          "Left side Conditional Result:%7.4f\n"
          "Right side Conditional Result:%7.4f\n"
          "Evaluate result: %s\n", 
          f1, f2, (state) ? t_TRUE : t_FALSE
        );
      }

      PVideoFrame dst = (state) ? source1->GetFrame(min(vi1.num_frames-1,n),env) : source2->GetFrame(min(vi2.num_frames-1,n),env);
      env->MakeWritable(&dst);
      env->ApplyMessage(&dst, vi, text, vi.width/4, 0xa0a0a0, 0, 0);

    return dst;
  }

  if (state) 
    return source1->GetFrame(min(vi1.num_frames-1,n),env);
  
  return source2->GetFrame(min(vi1.num_frames-1,n),env);
}

void __stdcall ConditionalFilter::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {
  source1->GetAudio(buf, start, count, env);
}


AVSValue __cdecl ConditionalFilter::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  intptr_t userdata = (intptr_t)user_data;
  if (userdata == 0)
    return new ConditionalFilter(args[0].AsClip(), args[1].AsClip(), args[2].AsClip(), args[3], args[4], args[5], args[6].AsBool(false),env);
  else // like GConditional filter shortcut: no "=" "true" needed
    return new ConditionalFilter(args[0].AsClip(), args[1].AsClip(), args[2].AsClip(), args[3], "=", "true", args[4].AsBool(false), env);
}


/**************************
 * ScriptClip.
 *
 * Returns the value of a script evaluated at each frame.
 *
 * Implicit last, and current frame is set on each frame.
 **************************/

ScriptClip::ScriptClip(PClip _child, AVSValue  _script, bool _show, bool _only_eval, bool _eval_after_frame, IScriptEnvironment* env) :
  GenericVideoFilter(_child), script(_script), show(_show), only_eval(_only_eval), eval_after(_eval_after_frame) {

}


int __stdcall ScriptClip::SetCacheHints(int cachehints, int frame_range)
{
#ifdef DEBUG_GSCRIPTCLIP_MT
  switch (cachehints) {
  case CACHE_DONT_CACHE_ME:
    return 1;
  case CACHE_GET_MTMODE:
    return MT_NICE_FILTER;
  default:
    return 0;
  }
#else
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
#endif
}

PVideoFrame __stdcall ScriptClip::GetFrame(int n, IScriptEnvironment* env) {
  AVSValue prev_last = env->GetVarDef("last");  // Store previous last
  env->SetVar("last",(AVSValue)child);       // Set explicit last

  if (show) {
    PVideoFrame dst = child->GetFrame(n,env);
    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi, script.AsString(), vi.width/6, 0xa0a0a0, 0, 0);
    env->SetVar("last",prev_last);       // Restore implicit last
    return dst;
  }

  AVSValue result;
  PVideoFrame eval_return;   // Frame to be returned if script should be evaluated AFTER frame has been fetched. Otherwise not used.

  if (eval_after) eval_return = child->GetFrame(n,env);

  try {
    ScriptParser parser(env, script.AsString(), "[ScriptClip]", n);
    PExpression exp = parser.Parse();
    result = exp->Evaluate(env);
  } catch (const AvisynthError &error) {    
    const char* error_msg = error.msg;  

    PVideoFrame dst = child->GetFrame(n,env);
    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi, error_msg, vi.width/W_DIVISOR, 0xa0a0a0, 0, 0);
    env->SetVar("last",prev_last);       // Restore implicit last
    return dst;
  }

  env->SetVar("last",prev_last);       // Restore implicit last

  if (eval_after && only_eval) return eval_return;
  if (only_eval) return child->GetFrame(n,env);
  
  const char* error = NULL;
  VideoInfo vi2 = vi;
  if (!result.IsClip()) {
    if (result.IsBool())
      error = "ScriptClip: Function did not return a video clip! (Was a bool)";
    else if (result.IsInt())
      error = "ScriptClip: Function did not return a video clip! (Was an int)";
    else if (result.IsFloat())
      error = "ScriptClip: Function did not return a video clip! (Was a float)";
    else if (result.IsString())
      error = "ScriptClip: Function did not return a video clip! (Was a string)";
    else if (result.IsArray())
      error = "ScriptClip: Function did not return a video clip! (Was an array)";
    else if (!result.Defined())
      error = "ScriptClip: Function did not return a video clip! (Was the undefined value)";
    else
      error = "ScriptClip: Function did not return a video clip! (type is unknown)";
  } else {
    vi2 = result.AsClip()->GetVideoInfo();
    if (!vi.IsSameColorspace(vi2)) { 
      error = "ScriptClip: Function did not return a video clip of the same colorspace as the source clip!";
    } else if (vi2.width != vi.width) {
      error = "ScriptClip: Function did not return a video clip with the same width as the source clip!";
    } else if (vi2.height != vi.height) {
      error = "ScriptClip: Function did not return a video clip with the same height as the source clip!";
    }
  }

  if (error != NULL) {
    PVideoFrame dst = child->GetFrame(n,env);
    env->MakeWritable(&dst);
    env->ApplyMessage(&dst, vi, error, vi.width/W_DIVISOR, 0xa0a0a0, 0, 0);
    return dst;
  }

  n = min(n,vi2.num_frames-1);  // We ignore it if the new clip is not as long as the current one. This can allow the resulting clip to be one frame.

  return result.AsClip()->GetFrame(n,env);
}


AVSValue __cdecl ScriptClip::Create(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new ScriptClip(args[0].AsClip(), args[1], args[2].AsBool(false),false, args[3].AsBool(false), env);
}


AVSValue __cdecl ScriptClip::Create_eval(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  return new ScriptClip(args[0].AsClip(), args[1], args[2].AsBool(false),true, args[3].AsBool(false), env);}
