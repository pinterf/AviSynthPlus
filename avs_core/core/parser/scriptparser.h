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

#ifndef __ScriptParser_H__
#define __ScriptParser_H__

#include <avisynth.h>
#include "expression.h"
#include "tokenizer.h"
#include "script.h"


/********************************************************************
********************************************************************/



class ScriptParser 
/**
  * Insert intelligent comment here
 **/
{
public:
  ScriptParser(IScriptEnvironment* _env, const char* _code, const char* _filename, int _frame_number);

  PExpression Parse(void);

  enum {max_args=1024};

private:
  IScriptEnvironment2* const env;
  Tokenizer tokenizer;
  const char* const code;
  const char* const filename;
  int loopDepth;    // how many loops are we in during parsing
  int frame_number; // for per-frame expressions

  void Expect(int op, const char* msg);

  void ParseFunctionDefinition(void);
  
  PExpression ParseBlock(bool braced, bool *empty);
  PExpression ParseStatement(bool* stop);
  PExpression ParseAssignment(void);
  PExpression ParseConditional(void);
  PExpression ParseOr(void);
  PExpression ParseAnd(void);
  PExpression ParseComparison(void);
  PExpression ParseAddition(bool negationOnHold);
  PExpression ParseMultiplication(bool negationOnHold);
  PExpression ParseUnary(void);
  PExpression ParseOOP(void);
#ifdef NEW_AVSVALUE
  PExpression ParseFunction(PExpression context, int frame_number, char context_char);
#else
  PExpression ParseFunction(PExpression context, int frame_number);
#endif
  PExpression ParseAtom(void);

  PExpression ParseIf(void);
  PExpression ParseWhile(void);
  PExpression ParseFor(void);

  // helper for ParseComparison
  int GetTokenAsComparisonOperator();
};




#endif  // __ScriptParser_H__