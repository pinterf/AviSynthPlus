#include "FilterConstructor.h"
#include "avisynth.h"

FilterConstructor::FilterConstructor(IScriptEnvironment2 * env,
  IScriptEnvironment_Avs25* env25,
  IScriptEnvironment_AvsPreV11C* envPreV11C,
  const Function *func, std::vector<AVSValue>* argStorage, std::vector<AVSValue>* ctorArgs) :
  Env(env),
  Env25(env25),
  EnvPreV11C(envPreV11C),
  Func(func),
#if 1
  ArgStorage(std::move(*argStorage)),
  CtorArgs(std::move(*ctorArgs))
#else
  // no need to move, subarrays could not be returned
  ArgStorage(*argStorage),
  CtorArgs(*ctorArgs)
#endif
{
}


// isPluginAvs25 and isPluginPreV11C cannot accept long and double types in
// their parameters passed to them.
// Such plugins don't recognize 'l' and 'd' 64-bit types due to direct type field checks in AVSValue/AVS_Value.
// IsInt/avs_is_int and IsFloat/avs_is_float check the type field directly in the old header, without an interface call.
// New avisynth_c.h supports these types, but old plugins don't.
// Thus we convert 64-bit types to 32-bit ones:
//   long int64 -> 32 bit int,
//   double -> float.
// In place: the arrays are already deep copied (owned) when funcArgs is built.
// Every AVSValue copy (copy constructor, operator=, array constructor) deep copies the whole array
// tree, so v and the array elements are taken by reference, only the 'l' and 'd' leaves are
// overwritten. A by-value parameter or 'AVSValue e = v[i]' would convert a copy, then throw it away.
// Changing the elements is safe: funcArgs is a private copy, the caller's arrays stay untouched.
// deep: array elements as well, at any depth (pre-V11 C plugins). Not for 2.5 plugins, which
// do not know deep copied arrays.
void ValueTo32bit(AVSValue& v, bool deep)
{
  switch (v.GetType()) {
  case AvsValueType::VALUE_TYPE_LONG:
    v = v.AsInt(); // same object, a scalar: no array copy (32 bit Avisynth: frees the int64 storage)
    break;
  case AvsValueType::VALUE_TYPE_DOUBLE:
    v = v.AsFloatf();
    break;
  case AvsValueType::VALUE_TYPE_ARRAY:
    if (deep) {
      // operator[] returns a const reference to the stored element. const_cast keeps it a reference:
      // the elements are not const objects (allocated by new AVSValue[] in Assign2).
      for (int i = 0; i < v.ArraySize(); i++)
        ValueTo32bit(const_cast<AVSValue&>(v[i]), true);
    }
    break;
  default:
    break;
  }
}

// helper called from InstantiateFilter and from the runtime (is_runtime) branch of ScriptEnvironment::Invoke_
void ArgsTo32bit(AVSValue& args, bool deep)
{
  for (int i = 0; i < args.ArraySize(); i++)
    ValueTo32bit(const_cast<AVSValue&>(args[i]), deep);
}

AVSValue FilterConstructor::InstantiateFilter() const
{
  // funcArgs is passed to the function as an array.
  // Calls the plugin's instance creator, which is usually Filter_Create(AVSValue args, void *user_data, IScriptEnvironment *env)
  // or create_c_filter
  AVSValue funcArgs(CtorArgs.data(), (int)CtorArgs.size());
  if (Func->isPluginAvs25 || Func->isPluginPreV11C)
    ArgsTo32bit(funcArgs, Func->isPluginPreV11C);

  AVSValue retval = Func->apply(funcArgs, Func->user_data, 
    Func->isPluginAvs25 ? (IScriptEnvironment *)Env25 : 
    Func->isPluginPreV11C ? (IScriptEnvironment*)EnvPreV11C : 
    Env);
  // pass back proper ScriptEnvironment, because a 2.5 plugin e.g. GRunT can back-Invoke ScriptClip
  if (Func->isPluginAvs25)
  {
    // After instantiate a v2.5 "baked code" function,
    // manually release Clips or else clip variables get stuck in memory on exit
    if (funcArgs.IsArray())
    {
      for (int i = 0; i < funcArgs.ArraySize(); i++)
      {
        if (funcArgs[i].IsClip()) {
          IClip* rawClip = (IClip*)(void*)funcArgs[i].AsClip(); // de-smart pointering
          rawClip->Release();
        }
      }
    }
    // Problem: AVS 2.5 plugins have "baked code", their interface contains direct manipulation, refcount, free of AVSValue resources.
    // So they do not know about array deep copy and deep free mechanism.
    // Thus array sub-elements of the parameter AVSValue won't be freed up on exiting of such functions.
    // Example: CtorArgs contains clip(s) with reference count N
    // Upon creating funcArgs the "smart" array creation will copy the subarray as well.
    // Because of the deep copy the reference count of these PClip values are increased by 1.
    // Then funcArgs will be passed to the CPP 2.5 function which won't release the Array content, only destroys the
    // 'holder' array-typed AVSValue, array elements will be untouched.
    // So if there is a PClip parameters the Clip's reference count remains N+1, causing memory leak.
    // This is why we have to release them manually, deferencing the Clip by one, instead of the plugin's code.
    // If this reference count decrease is not done then env->DeleteScriptEnvironment will not free up everything.
    // ScriptEnvironment destroy is freeing up variables as well.
    // If a variable contains such a clip (e.g. "last"), it will fail to be released, because such Clip will not reached refcount==0.
    // e.g. VirtualDub consecutively press "script reload"s. Memory will leak even 50-80Mbytes so after a while
    // memory gets full.
  }
  return retval;
}
