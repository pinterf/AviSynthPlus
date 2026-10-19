// Avisynth v2.6.  Copyright 2002-2009 Ben Rudiak-Gould et al.
// http://avisynth.nl

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

#include "MTGuard.h"
#include "cache.h"
#include "internal.h"
#include "FilterConstructor.h"
#include "InternalEnvironment.h"
#include <cassert>
#include <mutex>

#ifdef X86_32
#include <mmintrin.h>
#endif

struct MTGuardChildFilter {
  PClip filter;
  std::mutex mutex;
};

#ifdef AVS_WINDOWS
MTGuard::MTGuard(PClip firstChild, MtMode mtmode, std::unique_ptr<const FilterConstructor>&& funcCtor, const wchar_t* current_directory, InternalEnvironment* env) :
#else
MTGuard::MTGuard(PClip firstChild, MtMode mtmode, std::unique_ptr<const FilterConstructor>&& funcCtor, const char* current_directory, InternalEnvironment* env) :
#endif
  Env(env),
  nThreads(1),
  mt_enabled(false),
  FilterCtor(std::move(funcCtor)),
#ifdef AVS_WINDOWS
  CurrentDirectory(current_directory ? current_directory : L""),
#else
  CurrentDirectory(current_directory ? current_directory : ""),
#endif
  MTMode(mtmode)
{
  assert( ((int)mtmode > (int)MT_INVALID) && ((int)mtmode < (int)MT_MODE_COUNT) );

  ChildFilters = std::unique_ptr<MTGuardChildFilter[]>(new MTGuardChildFilter[1]);
  ChildFilters[0].filter = firstChild;
  vi = ChildFilters[0].filter->GetVideoInfo();

  Env->ManageCache(MC_RegisterMTGuard, reinterpret_cast<void*>(this));
}

MTGuard::~MTGuard()
{
  Env->ManageCache(MC_UnRegisterMTGuard, reinterpret_cast<void*>(this));
}

void MTGuard::EnableMT(size_t nThreads)
{
  // called for each filter in the chain, starting from the top-level filter
  // even if their mt_enabled were set by an earlier Prefetch.

  assert(nThreads >= 1);

  if (nThreads > 1)
  {
    switch (MTMode)
    {
    case MT_NICE_FILTER:
    {
      // already created single instance, just set the thread count
      if (!this->mt_enabled)
        ChildFilters[0].filter->SetCacheHints(CACHE_INFORM_NUM_THREADS, (int)nThreads);
      break;
    }
    case MT_MULTI_INSTANCE:
    {
      // creates the extra filter instances needed for the actual thread count
      // set only when unset
      if (!this->mt_enabled) {
        auto newchilds = std::unique_ptr<MTGuardChildFilter[]>(new MTGuardChildFilter[nThreads]);
        // copy existing
        for (size_t i = 0; i < this->nThreads; ++i) {
          newchilds[i].filter = ChildFilters[i].filter;
        }
        if (CurrentDirectory.empty()) {
          // create the rest
          for (size_t i = this->nThreads; i < nThreads; ++i) {
            newchilds[i].filter = FilterCtor->InstantiateFilter().AsClip();
          }
        }
        else {
          // switch to the original CurrentDirectory to instantiate the threaded filters
          // in order to their constructor to be able to see the original current directory
          CWDChanger change_cwd(CurrentDirectory.c_str()); // wstring on Windows, string on Linux

          // create the rest
          for (size_t i = this->nThreads; i < nThreads; ++i) {
            newchilds[i].filter = FilterCtor->InstantiateFilter().AsClip();
          }
        }
        // inform all filter instances about the threading
        for (size_t i = 0; i < nThreads; ++i)
          newchilds[i].filter->SetCacheHints(CACHE_INFORM_NUM_THREADS, (int)nThreads);
        ChildFilters = std::move(newchilds);
      }
      break;
    }
    case MT_SERIALIZED:
    {
      // already created single instance, just set the thread count
      if (!this->mt_enabled)
        ChildFilters[0].filter->SetCacheHints(CACHE_INFORM_NUM_THREADS, 1);
      break;
    }
    default:
    {
      assert(0);
      break;
    }
    }
  }
  else if (nThreads == 1) {
    if (!this->mt_enabled)
      ChildFilters[0].filter->SetCacheHints(CACHE_INFORM_NUM_THREADS, 1);
  }

  if (!this->mt_enabled) {
    this->nThreads = std::max(this->nThreads, nThreads);
    this->mt_enabled = true;
  }

  // We don't need the stored parameters any more,
  // free their memory.
  //FilterCtor.reset();
}

PVideoFrame __stdcall MTGuard::GetFrame(int n, IScriptEnvironment* env_)
{
  assert(nThreads > 0);
  /*
  // We can't call child filter without mutex guards even when nThreads is 1,
  // because a lately invoked filter may call GetFrame again, see
  // MT_SERIALIZED considerations below.
  // Code left here intentionally but commented out.
  if (nThreads == 1)
    return ChildFilters[0].filter->GetFrame(n, env);
  */
  InternalEnvironment* IEnv = GetAndRevealCamouflagedEnv(env_);
  IScriptEnvironment* env = static_cast<IScriptEnvironment*>(IEnv);

  PVideoFrame frame = NULL;

  switch (MTMode)
  {
  case MT_NICE_FILTER:
  {
    frame = ChildFilters[0].filter->GetFrame(n, env);
    break;
  }
  case MT_MULTI_INSTANCE:
  {
    // When called from thread pool then thread IDs are one-to-one mapped to ChildFilters array.
    // 'modulo' method ensures that no over-addressing can happen.
    // The number of filter instances are created in EnableMT as such.
    // Thread pool thread IDs are consecutive numbers, starting with 1.
    // Prefetch threads are consecutive numbers, starting with 0.
    // Here we cannot differentiate from where was the filter called.
    // Note: 
    //   when called from Prefetcher (which has 'num_of_logical_processors' threads instead of nThreads)
    //   the mapping is not ideal. It can be less or more than the actual instance count.
    //   E.g. when mapping a thread on a CPU with 8 logical cores 
    //   to a instance count of 3 (nThread=3) the mapping is uneven (modulo example): [0,3,6]->[0] [1,4,7]->[1] [2,5]->[2]
    size_t clipIndex = IEnv->GetThreadId() % nThreads;
    auto& child = ChildFilters[clipIndex];
    std::lock_guard<std::mutex> lock(child.mutex);
    frame = child.filter->GetFrame(n, env);
    break;
  }
  case MT_SERIALIZED:
  {
    std::lock_guard<std::mutex> lock(ChildFilters[0].mutex);
    // Deadlock situation when GetFrame(0) was called from an Invoke
    // solved in 3.7.2 by separating memory_mutex from invoke_mutex.
    frame = ChildFilters[0].filter->GetFrame(n, env);
    break;
  }
  default:
  {
    assert(0);
    env->ThrowError("Invalid Avisynth logic.");
    break;
  }
  } // switch

#ifdef X86_32
  _mm_empty();
#endif

  return frame;
}

void __stdcall MTGuard::GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env_)
{
  assert(nThreads > 0);

  /* commented out, see MTGuard::GetFrame comments for MT_SERIALIZED
  if (nThreads == 1)
  {
    ChildFilters[0].filter->GetAudio(buf, start, count, env);
    return;
  }
  */
  InternalEnvironment* IEnv = GetAndRevealCamouflagedEnv(env_);
  IScriptEnvironment* env = static_cast<IScriptEnvironment*>(IEnv);


  switch (MTMode)
  {
  case MT_NICE_FILTER:
    {
      ChildFilters[0].filter->GetAudio(buf, start, count, env);
      break;
    }
  case MT_MULTI_INSTANCE:
    {
      size_t clipIndex = IEnv->GetThreadId() % nThreads;
      auto& child = ChildFilters[clipIndex];
      std::lock_guard<std::mutex> lock(child.mutex);
      child.filter->GetAudio(buf, start, count, env);
      break;
    }
  case MT_SERIALIZED:
    {
      std::lock_guard<std::mutex> lock(ChildFilters[0].mutex);
      ChildFilters[0].filter->GetAudio(buf, start, count, env);
      break;
    }
  default:
    {
      assert(0);
      env->ThrowError("Invalid Avisynth logic.");
      break;
    }
  } // switch

#ifdef X86_32
  _mm_empty();
#endif
}

const VideoInfo& __stdcall MTGuard::GetVideoInfo()
{
  return vi;
}

bool __stdcall MTGuard::GetParity(int n)
{
  return ChildFilters[0].filter->GetParity(n);
}

// IF v13.1
int MTGuard::ForwardUserCacheHint(int cachehints, int frame_range)
{
  switch (MTMode)
  {
  case MT_NICE_FILTER:
  {
    // passed to the single instance
    auto& child = ChildFilters[0];
    return (child.filter->GetVersion() >= 5) ? child.filter->SetCacheHints(cachehints, frame_range) : 0;
  }
  case MT_MULTI_INSTANCE:
  case MT_SERIALIZED:
  {
    // MT_SERIALIZED: lock like in GetFrame/GetAudio
    // MT_MULTI_INSTANCE special: passed to every instance, each call has its own lock
    // Note: instances that are created by EnableMT (Prefetch) later, will not receive earlier hints
    int result = 0;
    const size_t instances = (MTMode == MT_MULTI_INSTANCE) ? nThreads : 1;
    for (size_t i = 0; i < instances; ++i) {
      auto& child = ChildFilters[i];
      std::lock_guard<std::mutex> lock(child.mutex);
      const int res = (child.filter->GetVersion() >= 5) ? child.filter->SetCacheHints(cachehints, frame_range) : 0;
      // result from index 0 only
      if (i == 0) // MT_MULTI_INSTANCE: return the result of the first instance, as the others may throw
        result = res;
    }
    return result;
  }
  default:
    assert(0);
    throw AvisynthError("Invalid Avisynth logic.");
  }
}

int __stdcall MTGuard::SetCacheHints(int cachehints, int frame_range)
{
  if (IsForwardedUserCacheHint(cachehints))
    return ForwardUserCacheHint(cachehints, frame_range); // GetVersion>=5 check inside

  if (CACHE_GET_MTMODE == cachehints) {
    return MT_NICE_FILTER;
  }
  if (CACHE_IS_MTGUARD_REQ == cachehints) {
    return CACHE_IS_MTGUARD_ANS;
  }
  // Filter-MTGuard-CacheGuard(Cache or nothing)
  // MTGuard is just a helper over real filters, we pass these requests upstream
  if (CACHE_GET_AUDIO_POLICY == cachehints || CACHE_GET_AUDIO_SIZE == cachehints ||
    CACHE_GETCHILD_AUDIO_MODE == cachehints || CACHE_GETCHILD_AUDIO_SIZE == cachehints)
    return (ChildFilters[0].filter->GetVersion() >= 5) ? ChildFilters[0].filter->SetCacheHints(cachehints, 0) : 0;
  if (CACHE_GET_DEV_TYPE == cachehints || CACHE_GET_CHILD_DEV_TYPE == cachehints) {
    return (ChildFilters[0].filter->GetVersion() >= 5) ? ChildFilters[0].filter->SetCacheHints(cachehints, 0) : 0;
  }

  return 0;
}

bool __stdcall MTGuard::IsMTGuard(const PClip& p)
{
  return ((p->GetVersion() >= 5) && (p->SetCacheHints(CACHE_IS_MTGUARD_REQ, 0) == CACHE_IS_MTGUARD_ANS));
}

#ifdef AVS_WINDOWS
PClip MTGuard::Create(MtMode mode, PClip filterInstance, std::unique_ptr<const FilterConstructor> funcCtor, const wchar_t* current_directory, InternalEnvironment* env)
#else
PClip MTGuard::Create(MtMode mode, PClip filterInstance, std::unique_ptr<const FilterConstructor> funcCtor, const char* current_directory, InternalEnvironment* env)
#endif
{
    switch (mode)
    {
    case MT_NICE_FILTER:
    {
      // Put a guard even around MT_NICE_FILTER mode filters, in order EnableMT to
      // be able to inform the filter (CACHE_SET_NUM_OF_THREAD) about the actual (Prefetch) thread count.
      return new MTGuard(filterInstance, mode, nullptr, nullptr, env);
    }
    case MT_MULTI_INSTANCE: // Fall-through intentional
    {
        return new MTGuard(filterInstance, mode, std::move(funcCtor), current_directory, env);
        // args2 and args3 are not valid after this point anymore
    }
    case MT_SERIALIZED:
    {
      // Put a guard even around MT_SERIALIZED mode filters, in order EnableMT to
      // be able to inform the filter (CACHE_SET_NUM_OF_THREAD) about the actual
      // (1) thread count.
        return new MTGuard(filterInstance, mode, nullptr, nullptr, env);
        // args2 and args3 are not valid after this point anymore
    }
    default:
        // There are broken plugins out there in the wild that have (GetVersion() >= 5), but still
        // return garbage for SetCacheHints(). However, this case should be recognized and
        // handled earlier, so we can never get to this default-branch. If we do, assume the worst.
        assert(0);
        return new MTGuard(filterInstance, MT_SERIALIZED, nullptr, nullptr, env);
    }
}
