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


#include "scriptparser.h"


/********************************
 *******   Script Parser   ******
 *******************************/
 

ScriptParser::ScriptParser(IScriptEnvironment* _env, const char* _code, const char* _filename, int _frame_number)
   : env(static_cast<IScriptEnvironment2*>(_env)), tokenizer(_code, _env), code(_code), filename(_filename), loopDepth(0), frame_number(_frame_number) {}

PExpression ScriptParser::Parse(void) 
{
  try {
    return new ExpRootBlock(ParseBlock(false, NULL));
  }
  catch (const AvisynthError &ae) {
    env->ThrowError("%s\n(%s, line %d, column %d)", ae.msg, filename, tokenizer.GetLine(), tokenizer.GetColumn(code));
  }
#ifndef _DEBUG
  catch (...) {
    env->ThrowError("Parse: Unrecognized exception!");
  }
#endif
  return 0; // To make VC++ happy.  Why isn't the __declspec(noreturn) on ThrowError good enough?
}


void ScriptParser::Expect(int op, const char* msg=0) 
{
  if (tokenizer.IsOperator(op))
    tokenizer.NextToken();
  else {
    if (msg)
      env->ThrowError(msg);
    else {
      if (op < 256)
        env->ThrowError("Script error: expected `%c'", op);
      else
        env->ThrowError("Script error: expected `%c%c'", (op>>8), (op&255));
    }
  }
}


void ScriptParser::ParseFunctionDefinition(void) 
{
  if (!tokenizer.IsIdentifier())
    env->ThrowError("Script error: expected a function name");
  const char* name = tokenizer.AsIdentifier();
  tokenizer.NextToken();
  char param_types[4096];
  int param_chars=0;
  bool param_floats[max_args];
  const char* param_names[max_args];
  int param_count=0;
  if (!tokenizer.IsOperator('{')) {
    Expect('(', "Script error: expected ( or { after function name");
    bool need_comma = false;
    bool named_arg_found = false;
    for (;;) {
      if (tokenizer.IsOperator(')')) {
         tokenizer.NextToken();
        break;
      }
      if (need_comma) {
        Expect(',', "Script error: expected a , or )");
      }
      if (param_count == max_args) {
        env->ThrowError("Script error: parameter list too long");
      }

      param_floats[param_count] = false;
      char type = '.';
      Tokenizer lookahead(&tokenizer);
      if (lookahead.IsIdentifier() || lookahead.IsString()) {
        // we have a variable type preceding its name
        if (tokenizer.IsIdentifier("val")) type = '.';
        else if (tokenizer.IsIdentifier("bool")) type = 'b';
        else if (tokenizer.IsIdentifier("int")) type = 'i';
        else if (tokenizer.IsIdentifier("float")) {
          param_floats[param_count] = true;
          type = 'f';
        }
        else if (tokenizer.IsIdentifier("string")) type = 's';
        else if (tokenizer.IsIdentifier("clip")) type = 'c';
#ifdef NEW_AVSVALUE
        else if (tokenizer.IsIdentifier("array")) type = 'a'; // AVS+ 161028 array type in user defined functions
#endif
        else env->ThrowError("Script error: expected \"val\", \"bool\", \"int\", \"float\", \"string\", \"array\", or \"clip\"");
        tokenizer.NextToken();
      }

      if (tokenizer.IsIdentifier()) {
        if (named_arg_found)
          env->ThrowError("Script error: can't have a named (quoted) parameter followed by an ordinary parameter");
        param_names[param_count++] = tokenizer.AsIdentifier();
      } else if (tokenizer.IsString()) {
        named_arg_found = true;
        const char* param_name = param_names[param_count++] = tokenizer.AsString();
        int len = lstrlen(param_name);
        if (param_chars + lstrlen(param_name) >= 4000)
          env->ThrowError("Script error: parameter list too long");
        param_types[param_chars] = '[';
        memcpy(&param_types[param_chars+1], param_name, len);
        param_types[param_chars+len+1] = ']';
        param_chars += len+2;
      } else {
        env->ThrowError("Script error: expected a parameter name");
      }
      param_types[param_chars++] = type;
      tokenizer.NextToken();

      need_comma = true;
    }
  }

  param_types[param_chars] = 0;
  PExpression body = new ExpRootBlock(ParseBlock(true, NULL));
  ScriptFunction* sf = new ScriptFunction(body, param_floats, param_names, param_count);
  env->AtExit(ScriptFunction::Delete, sf);
  env->AddFunction(name, env->SaveString(param_types), ScriptFunction::Execute, sf, "$UserFunctions$");
}


PExpression ScriptParser::ParseBlock(bool braced, bool *empty) 
{
  if (braced) {
    // allow newlines (and hence comments) before '{' -- Gavino 7 Dec 2009
    while (tokenizer.IsNewline())
      tokenizer.NextToken();

    Expect('{');
  }

  // the purpose of this array and the accompanying code is to produce
  // a nice balanced binary tree of ExpSequence objects, so that the
  // maximum call depth in Evaluate grows logarithmically instead of
  // linearly.
  // For every i, either trees[i]==0 or it's a balanced tree of (1<<i) elts.
  PExpression trees[20];

  bool ignore_remainder = false;
  for (;;) {
    if (tokenizer.IsNewline()) {
      tokenizer.NextToken();
    } else if (tokenizer.IsOperator('}')) {
      if (braced) {
        tokenizer.NextToken();
        break;
      } else {
        env->ThrowError("Script error: found } without a matching {");
      }
    } else if (tokenizer.IsEOF()) {
      if (braced) {
        env->ThrowError("Script error: end of file reached without matching }");
      } else {
        break;
      }
    } else {
      bool stop;
      PExpression exp = ParseStatement(&stop);
      if (exp && !ignore_remainder) {
        if (filename)
          exp = new ExpLine(exp, filename, tokenizer.GetLine());
        for (int i=0; i<20; ++i) {
          if (trees[i]) {
            exp = new ExpSequence(trees[i], exp);
            trees[i] = 0;
          } else {
            trees[i] = exp;
            break;
          }
        }
      }
      ignore_remainder |= stop;
    }
  }

  PExpression result = trees[0];
  for (int i=1; i<20; ++i) {
    if (trees[i])
      result = result ? PExpression(new ExpSequence(trees[i], result)) : trees[i];
  }

  if (result)
  {
    if (empty) *empty = false;
    return result;
  }
  else
  {
    if (empty) *empty = true;
    return PExpression(new ExpConstant(AVSValue()));
  }
}



PExpression ScriptParser::ParseStatement(bool* stop) 
{
  *stop = false;
  // null statement
  if (tokenizer.IsNewline() || tokenizer.IsEOF()) {
    return 0;
  }
  // function declaration
  else if (tokenizer.IsIdentifier("function")) {
    tokenizer.NextToken();
    ParseFunctionDefinition();
    return 0;
  }
  // exception handling
  else if (tokenizer.IsIdentifier("try")) {
    tokenizer.NextToken();
    PExpression try_block = ParseBlock(true, NULL);
    while (tokenizer.IsNewline())
      tokenizer.NextToken();
    if (!tokenizer.IsIdentifier("catch"))
      env->ThrowError("Script error: expected `catch'");
    tokenizer.NextToken();
    Expect('(');
    if (!tokenizer.IsIdentifier())
      env->ThrowError("Script error: expected identifier");
    const char* id = tokenizer.AsIdentifier();
    tokenizer.NextToken();
    Expect(')');
    return new ExpTryCatch(try_block, id, ParseBlock(true, NULL));
  }
  // 'if', 'while', 'for':
  else if (tokenizer.IsIdentifier("if")) {
    return ParseIf();
  }
  else if (tokenizer.IsIdentifier("while")) {
    return ParseWhile();
  }
  else if (tokenizer.IsIdentifier("for")) {
    return ParseFor();
  }
  // return statement
  else if (tokenizer.IsIdentifier("return")) {
    *stop = true;
    tokenizer.NextToken();
    return new ExpReturn(ParseConditional());
  }
  // break statement
  else if (tokenizer.IsIdentifier("break")) {
    if (loopDepth <= 0)
      throw AvisynthError("'Break' statement outside of loop.");
    tokenizer.NextToken();
    return new ExpBreak();
  }
  else {
    return ParseAssignment();
  }
}

PExpression ScriptParser::ParseIf(void) 
{  
  bool blockEmpty;

  PExpression If, Then, Else = 0;
  tokenizer.NextToken();
  Expect('(');
  If = ParseConditional();
  Expect(')');

  Then = ParseBlock(true, &blockEmpty);
  if (blockEmpty)
    Then = NULL;

  while (tokenizer.IsNewline())
    tokenizer.NextToken();
  if (tokenizer.IsIdentifier("else")) {
    tokenizer.NextToken();

    if (tokenizer.IsIdentifier("if"))
    {
      Else = ParseIf();
    }
    else
    {
      Else = ParseBlock(true, &blockEmpty);
      if (blockEmpty)
        Else = NULL;
    }
  }
  return new ExpBlockConditional(If, Then, Else);
}

PExpression ScriptParser::ParseWhile(void) 
{  
  tokenizer.NextToken();
  Expect('(');
  const PExpression cond = ParseConditional();
  Expect(')');

  ++loopDepth;
  bool blockEmpty;
  PExpression body = ParseBlock(true, &blockEmpty);
  if (blockEmpty)
    body = NULL;
  --loopDepth;

  return new ExpWhileLoop(cond, body);
}

PExpression ScriptParser::ParseFor(void) 
{  
  tokenizer.NextToken();
  Expect('(');
  if (!tokenizer.IsIdentifier())
    env->ThrowError("Script error: expected a variable name");
  const char* id = tokenizer.AsIdentifier();
  tokenizer.NextToken();
  Expect('=');
  const PExpression init = ParseConditional();
  Expect(',');
  const PExpression limit = ParseConditional();
  PExpression step = NULL;
  if (tokenizer.IsOperator(',')) {
    tokenizer.NextToken();
    step = ParseConditional();
  } else {
    step = PExpression(new ExpConstant(AVSValue(1)));
  }

  Expect(')');

  ++loopDepth;
  bool blockEmpty;
  PExpression body = ParseBlock(true, &blockEmpty);
  if (blockEmpty)
    body = NULL;
  --loopDepth;

  return new ExpForLoop(id, init, limit, step, body);
}

PExpression ScriptParser::ParseAssignment(void) 
{
  if (tokenizer.IsIdentifier("global")) {
    tokenizer.NextToken();
    if (!tokenizer.IsIdentifier())
      env->ThrowError("Script error: `global' must be followed by a variable name");
    const char* name = tokenizer.AsIdentifier();
    tokenizer.NextToken();
    Expect('=');
    PExpression exp = ParseConditional();
    return new ExpGlobalAssignment(name, exp);
  }
  PExpression exp = ParseConditional();
  if (tokenizer.IsOperator('=')) {
    const char* name = exp->GetLvalue();
    if (!name)
      env->ThrowError("Script error: left operand of `=' must be a variable name");
    tokenizer.NextToken();
    exp = ParseConditional();
    return new ExpAssignment(name, exp);
  }
  return exp;
}


PExpression ScriptParser::ParseConditional(void) 
{
  PExpression a = ParseOr();
  if (tokenizer.IsOperator('?')) {
    tokenizer.NextToken();
    PExpression b = ParseConditional();
    Expect(':');
    PExpression c = ParseConditional();
    return new ExpConditional(a, b, c);
  }
  return a;
}

PExpression ScriptParser::ParseOr(void) 
{
  PExpression left = ParseAnd();
  if (tokenizer.IsOperator('||')) {
    tokenizer.NextToken();
    PExpression right = ParseOr();
    return new ExpOr(left, right);
  }
  return left;
}

PExpression ScriptParser::ParseAnd(void) 
{
  PExpression left = ParseComparison();
  if (tokenizer.IsOperator('&&')) {
    tokenizer.NextToken();
    PExpression right = ParseAnd();
    return new ExpAnd(left, right);
  }
  return left;
}


PExpression ScriptParser::ParseComparison(void)
{
  PExpression left = ParseAddition(false);
  PExpression result;
  int op;
  while ((op = GetTokenAsComparisonOperator()) != 0) {
    tokenizer.NextToken();
    PExpression right = ParseAddition(false);
    PExpression term;
    switch (op) {
      case '==': term = new ExpEqual(left, right); break;
      case '!=': term = new ExpNot(new ExpEqual(left, right)); break;
      case '<>': term = new ExpNot(new ExpEqual(left, right)); break;
      case '<': term = new ExpLess(left, right); break;
      case '>=': term = new ExpNot(new ExpLess(left, right)); break;
      case '>': term = new ExpLess(right, left); break;
      case '<=': term = new ExpNot(new ExpLess(right, left)); break;
    }
    result = !result ? term : PExpression(new ExpAnd(result, term));
    left = right;
  }
  return result ? result : left;
}



PExpression ScriptParser::ParseAddition(bool negationOnHold) //update exterior calls to ParseAddition(false)
{
  PExpression left = ParseMultiplication(negationOnHold);
  bool plus = tokenizer.IsOperator('+');
  bool minus = tokenizer.IsOperator('-');
  bool doubleplus = tokenizer.IsOperator('++');
  if (plus || minus || doubleplus) {
    tokenizer.NextToken();
    PExpression right = ParseAddition(minus);
    if (doubleplus) {
      return new ExpDoublePlus(left, right);
    }
    return new ExpPlus(left, right);   //no longer ExpMinus  'right' will be negated when needed
  }
  return left;
}

PExpression ScriptParser::ParseMultiplication(bool negationOnHold) 
{
  PExpression left = ParseUnary();

  for (;;) {
    bool mult = tokenizer.IsOperator('*');
    bool div = tokenizer.IsOperator('/');
    bool mod = tokenizer.IsOperator('%');
    
    if (mult || div || mod)
      tokenizer.NextToken();
    else break;                                 //exits the while if not a mult op
 
    PExpression right = ParseUnary();
    if (mult)
      left = new ExpMult(left, right);
    else if (div)
      left = new ExpDiv(left, right);
    else
      left = new ExpMod(left, right);
  }

  if (negationOnHold)   //negate the factorised result if needed
    left = new ExpNegate(left);
  return left;
}


PExpression ScriptParser::ParseUnary(void) {
  // accept '+' with anything
  while (tokenizer.IsOperator('+'))
    tokenizer.NextToken();

  if (tokenizer.IsOperator('-')) {
    tokenizer.NextToken();
    return new ExpNegate(ParseUnary());
  }
  else if (tokenizer.IsOperator('!')) {
    tokenizer.NextToken();
    return new ExpNot(ParseUnary());
  }
  else {
    return ParseOOP();
  }
}

PExpression ScriptParser::ParseOOP(void) 
{
#ifndef NEW_AVSVALUE
  PExpression left = ParseFunction(0, frame_number);
  while (tokenizer.IsOperator('.')) {
    tokenizer.NextToken();
    left = ParseFunction(left, frame_number);
  }
#else
  PExpression left = ParseFunction(0, '\0');
  while (tokenizer.IsOperator('.') || tokenizer.IsOperator('[')) {
    // OOP '.' or array indexing
    char op = tokenizer.AsOperator();
    tokenizer.NextToken();
    left = ParseFunction(left, op);
  }
#endif
  return left;
}

#ifndef NEW_AVSVALUE
PExpression ScriptParser::ParseFunction(PExpression context, int frame_number)
#else
PExpression ScriptParser::ParseFunction(PExpression context, int frame_number, char context_char)
#endif
{
#ifndef NEW_AVSVALUE
  if (!tokenizer.IsIdentifier()) {
    if (context)
      env->ThrowError("Script error: expected function name following `.'");
    else
      return ParseAtom();
  }
#else
  bool isVariableReference = (context_char == '[');
  bool isArraySpecifier = isVariableReference || tokenizer.IsOperator('[');
  if (isArraySpecifier) // debug
  {
    isArraySpecifier = isArraySpecifier;
  }
  if (!tokenizer.IsIdentifier() && !isVariableReference) {
    if (context)
      env->ThrowError("Script error: expected function name following `.'");
    else if (!isArraySpecifier)
      return ParseAtom();
  }
#endif
#ifdef NEW_AVSVALUE
  // treat [ as special function: "Array"
  const char* name = (isArraySpecifier ) ? (isVariableReference ? "ArrayGet" : "Array") : tokenizer.AsIdentifier();
  if(!isArraySpecifier) // also for variable reference: ParseOOP already had [ and also the next
    tokenizer.NextToken();
#else
  const char* name = tokenizer.AsIdentifier();
  tokenizer.NextToken();
#endif

#ifndef NEW_AVSVALUE
  if (!context && !tokenizer.IsOperator('(')) {
#else
  if (!context && !tokenizer.IsOperator('(') && !isArraySpecifier) {
#endif
    // variable
    return new ExpVariableReference(name, frame_number);
  }
  // function
  PExpression args[max_args];
  const char* arg_names[max_args];
  memset(arg_names, 0, sizeof(arg_names));
  int params_count = 0;
  int i=0;
  if (context)
    args[i++] = context; // first arg is the object before '.'
  if (
#ifdef NEW_AVSVALUE
    isArraySpecifier ||
#endif
    tokenizer.IsOperator('(')) {
#ifdef NEW_AVSVALUE
    if(!isVariableReference) // ParseOOP already had [ and also the next token
#endif
      tokenizer.NextToken();
    bool need_comma = false;
    for (;;) {
#ifdef NEW_AVSVALUE
      if ((isArraySpecifier && tokenizer.IsOperator(']')) // arrays are delimited by ]
          || (!isArraySpecifier &&  tokenizer.IsOperator(')')))
#else
      if(tokenizer.IsOperator(')'))
#endif
      {
        tokenizer.NextToken();
        break;
      }
      if (need_comma) {
#ifdef NEW_AVSVALUE
        if(isArraySpecifier)
          Expect(',', "Script error: expected a , or ]");
        else
#endif
          Expect(',', "Script error: expected a , or )");
      }
      // check for named argument syntax (name=val)
      if (tokenizer.IsIdentifier()) {
        Tokenizer lookahead(&tokenizer);
        if (lookahead.IsOperator('=')) {
          arg_names[i] = tokenizer.AsIdentifier();
          tokenizer.NextToken();
          tokenizer.NextToken();
        }
      }
      if (i == max_args) {
        env->ThrowError("Script error: argument list too long");
      }
      args[i++] = ParseConditional();
      params_count++;
      need_comma = true;
    }
     // no this will be an one-element array of one empty array
    /*
    if (isArraySpecifier && params_count == 0)
    {
      // special case: empty array!
      args[i++] = new ExpConstant(AVSValue()); // undefined. Array() will create null-element array
    }
    */
  }
#ifdef NEW_AVSVALUE
  if (isVariableReference && params_count == 0)
  {
    env->ThrowError("Script error: array indexing must have at least one index");
  }
#endif
  return new ExpFunctionCall(name, args, arg_names, i, !!context, frame_number);
}

PExpression ScriptParser::ParseAtom(void)
{
  if (tokenizer.IsInt()) {
    int result = tokenizer.AsInt();
    tokenizer.NextToken();
    return new ExpConstant(result);
  }
  else if (tokenizer.IsFloat()) {
    float result = tokenizer.AsFloat();
    tokenizer.NextToken();
    return new ExpConstant(result);
  }
  else if (tokenizer.IsString()) {
    const char* result = tokenizer.AsString();
    tokenizer.NextToken();
    return new ExpConstant(result);
  }
#ifdef ARRAYS_AT_TOKENIZER_LEVEL
  else if (tokenizer.IsArray()) {
    std::vector<AVSValue>* result = tokenizer.AsArray(); // PF tokenizer returns new array
    tokenizer.NextToken();
    return new ExpConstant(result);
  }
#endif
  else if (tokenizer.IsOperator('(')) {
    tokenizer.NextToken();
    PExpression result = ParseConditional();
    Expect(')');
    return result;
  }
  else {
    env->ThrowError("Script error: syntax error");
    return 0;
  }
}

int ScriptParser::GetTokenAsComparisonOperator()
{
  if (!tokenizer.IsOperator())
    return 0;
  int op = tokenizer.AsOperator();
  if (op == '==' || op == '!=' || op == '<>' || op == '<' || op == '>' || op == '<=' || op == '>=')
    return op;
  else
    return 0;
}

