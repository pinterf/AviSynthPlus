#ifndef _AVS_FILTER_CONSTRUCTOR_H
#define _AVS_FILTER_CONSTRUCTOR_H

#include "internal.h"
#include <vector>

// Function arguments (an owned, deep copied array) for isPluginAvs25 and isPluginPreV11C plugins:
// long and double converted in place to 32 bit int and float. deep: array elements as well (pre-V11 C)
void ArgsTo32bit(AVSValue& args, bool deep);
// The same for a single value (e.g. avs_invoke results)
void ValueTo32bit(AVSValue& v, bool deep);

class FilterConstructor
{
private:
  IScriptEnvironment2* const Env;
  IScriptEnvironment_Avs25* const Env25;
  IScriptEnvironment_AvsPreV11C* const EnvPreV11C;
  const Function* const Func;
  const std::vector<AVSValue> ArgStorage;
  const std::vector<AVSValue> CtorArgs;

public:
  FilterConstructor(IScriptEnvironment2 * env,
    IScriptEnvironment_Avs25* env25,
    IScriptEnvironment_AvsPreV11C* envPreV11C,
    const Function *func, std::vector<AVSValue>* argStorage, std::vector<AVSValue>* ctorArgs);
  AVSValue InstantiateFilter() const;

  const char* GetFilterName() const
  {
    return Func->name;
  }

  bool IsScriptFunction() const
  {
    return AVSFunction::IsScriptFunction(Func);
  }

  const Function* GetAvsFunction() const
  {
      return Func;
  }
};

#endif  // _AVS_FILTER_CONSTRUCTOR_H
