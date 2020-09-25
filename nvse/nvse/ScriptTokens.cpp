#include "ScriptTokens.h"

#include <unordered_map>

#include "ScriptUtils.h"
#include "GameRTTI.h"

#ifdef DBG_EXPR_LEAKS
	SInt32 TOKEN_COUNT = 0;
	SInt32 EXPECTED_TOKEN_COUNT = 0;
#define INC_TOKEN_COUNT TOKEN_COUNT++;
#else
#define INC_TOKEN_COUNT
#endif

ScriptToken::~ScriptToken()
{
#ifdef DBG_EXPR_LEAKS
	TOKEN_COUNT--;
#endif
}

/*************************************

	ScriptToken constructors

*************************************/

// ScriptToken
ScriptToken::ScriptToken() : type(kTokenType_Invalid), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.num = 0;
}

ScriptToken::ScriptToken(Token_Type _type, UInt8 _varType, UInt16 _refIdx) : type(_type), variableType(_varType), refIdx(_refIdx) 
{ 
	INC_TOKEN_COUNT
}

ScriptToken::ScriptToken(bool boolean) : type(kTokenType_Boolean), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.num = boolean ? 1 : 0;
}

ScriptToken::ScriptToken(double num) : type(kTokenType_Number), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.num = num;
}

ScriptToken::ScriptToken(Script::RefVariable* refVar, UInt16 refIdx) : type(kTokenType_Ref), refIdx(refIdx), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.refVar = refVar;
}

ScriptToken::ScriptToken(const std::string& str) : type(kTokenType_String), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.str = str;
}

ScriptToken::ScriptToken(const char* str) : type(kTokenType_String), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.str = str;
}

ScriptToken::ScriptToken(TESGlobal* global, UInt16 refIdx) : type(kTokenType_Global), refIdx(refIdx), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.global = global;
}

ScriptToken::ScriptToken(UInt32 id, Token_Type asType) : refIdx(0), type(asType), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	switch (asType)
	{
	case kTokenType_Form:
		value.formID = id;
		break;
#if RUNTIME
	case kTokenType_Array:
		value.arrID = id;
		type = (!id || g_ArrayMap.Get(id)) ? kTokenType_Array : kTokenType_Invalid;
		break;
#endif
	default:
		type = kTokenType_Invalid;
	}
}

ScriptToken::ScriptToken(Operator* op) : type(kTokenType_Operator), refIdx(0), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.op = op;
}

#if RUNTIME
ScriptToken::ScriptToken(ExpressionEvaluator& evaluator)
{
	ReadFrom(&evaluator);
}
#endif

ScriptToken::ScriptToken(VariableInfo* varInfo, UInt16 refIdx, UInt32 varType) : refIdx(refIdx), variableType(varType)
{
	INC_TOKEN_COUNT
	value.varInfo = varInfo;
	switch (varType)
	{
	case Script::eVarType_Array:
		type = kTokenType_ArrayVar;
		break;
	case Script::eVarType_String:
		type = kTokenType_StringVar;
		break;
	case Script::eVarType_Integer:
	case Script::eVarType_Float:
		type = kTokenType_NumericVar;
		break;
	case Script::eVarType_Ref:
		type = kTokenType_RefVar;
		break;
	default:
		type = kTokenType_Invalid;
	}
}

ScriptToken::ScriptToken(CommandInfo* cmdInfo, UInt16 refIdx) : type(kTokenType_Command), refIdx(refIdx), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.cmd = cmdInfo;
}

#if RUNTIME
void ScriptToken::Delete() const
{
	if (!cached)
	{
		delete this;
	}
}
#endif

bool ScriptToken::IsInvalid() const
{
	return this->type == kTokenType_Invalid;
}

bool ScriptToken::IsOperator() const
{
	return this->type == kTokenType_Operator;
}

bool ScriptToken::IsLogicalOperator() const
{
	if (Type() != kTokenType_Operator)
		return false;
	const auto& type = GetOperator()->type;
	return type == kOpType_LogicalOr || type == kOpType_LogicalAnd;
}

#if RUNTIME
// ###TODO: Read() sets variable type; better to pass it to this constructor
ScriptToken::ScriptToken(ScriptEventList::Var* var) : refIdx(0), type(kTokenType_Variable), variableType(Script::eVarType_Invalid)
{
	INC_TOKEN_COUNT
	value.var = var;
}

ForEachContextToken::ForEachContextToken(UInt32 srcID, UInt32 iterID, UInt32 varType, ScriptEventList::Var* var)
: ScriptToken(kTokenType_ForEachContext, Script::eVarType_Invalid, 0), context(srcID, iterID, varType, var)
{
	value.formID = 0;
}

SmallObjectsAllocator::Allocator<ForEachContextToken, 4> g_forEachTokenAllocator;

void* ForEachContextToken::operator new(size_t size)
{
	return g_forEachTokenAllocator.Allocate();
}

void ForEachContextToken::operator delete(void* p)
{
	g_forEachTokenAllocator.Free(p);
}

ScriptToken* ScriptToken::Create(ForEachContext* forEach)
{
	if (!forEach)
		return NULL;

	if (forEach->variableType == Script::eVarType_String)
	{
		if (!g_StringMap.Get(forEach->iteratorID) || !g_StringMap.Get(forEach->sourceID))
			return NULL;
	}
	else if (forEach->variableType == Script::eVarType_Array)
	{
		if (!g_ArrayMap.Get(forEach->sourceID) || !g_ArrayMap.Get(forEach->iteratorID))
			return NULL;
	}
	else if (forEach->variableType == Script::eVarType_Ref)
	{
		TESObjectREFR* target = DYNAMIC_CAST((TESForm*)forEach->sourceID, TESForm, TESObjectREFR);
		if (!target)
			return NULL;
	}
	else
		return NULL;

	return new ForEachContextToken(forEach->sourceID, forEach->iteratorID, forEach->variableType, forEach->var);
}

ScriptToken* ScriptToken::Create(ArrayID arrID, ArrayKey* key)
{
	if (key) return new ArrayElementToken(arrID, key);
	else return NULL;
}

ScriptToken* ScriptToken::Create(Slice* slice)
{
	if (slice) return new SliceToken(slice);
	else return NULL;
}

ScriptToken* ScriptToken::Create(ScriptToken* l, ScriptToken* r)
{
	return new PairToken(l, r);
}

ScriptToken* ScriptToken::Create(UInt32 varID, UInt32 lbound, UInt32 ubound)
{
	return new AssignableSubstringStringVarToken(varID, lbound, ubound);
}

ScriptToken* ScriptToken::Create(ArrayElementToken* elem, UInt32 lbound, UInt32 ubound)
{
	
	if (elem && elem->GetString()) {
		return new AssignableSubstringArrayElementToken(elem->GetOwningArrayID(), *elem->GetArrayKey(), lbound, ubound);
	}

	return NULL;
}

static SmallObjectsAllocator::Allocator<ScriptToken, 32> g_scriptTokenAllocator;

void* ScriptToken::operator new(size_t size)
{
	//return ::operator new(size);
	return g_scriptTokenAllocator.Allocate();
}

void ScriptToken::operator delete(void* p)
{
	//::operator delete(p);
	g_scriptTokenAllocator.Free(p);
}

ArrayElementToken::ArrayElementToken(ArrayID arr, ArrayKey* _key)
: ScriptToken(kTokenType_ArrayElement, Script::eVarType_Invalid, 0)
{
	value.arrID = arr;
	key = *_key;
}

TokenPair::TokenPair(ScriptToken* l, ScriptToken* r) : left(NULL), right(NULL)
{
	if (l && r) {
		left = l->ToBasicToken();
		if (left) {
			right = r->ToBasicToken();
			if (!right) {
				delete left;
				left = NULL;
			}
		}
	}
}

TokenPair::~TokenPair()
{
	delete left;
	delete right;
}

PairToken::PairToken(ScriptToken* l, ScriptToken* r) : ScriptToken(kTokenType_Pair, Script::eVarType_Invalid, 0), pair(l,r)
{
	if (!pair.left || !pair.right)
		type = kTokenType_Invalid;
}

AssignableSubstringToken::AssignableSubstringToken(UInt32 _id, UInt32 lbound, UInt32 ubound) 
	:  ScriptToken(kTokenType_AssignableString, Script::eVarType_Invalid, 0), lower(lbound), upper(ubound), substring()
{
	value.arrID = _id;
	if (lower > upper) {
		lower = 0;
	}
}

AssignableSubstringStringVarToken::AssignableSubstringStringVarToken(UInt32 _id, UInt32 lbound, UInt32 ubound) : AssignableSubstringToken(_id, lbound, ubound)
{
	StringVar* strVar = g_StringMap.Get(value.arrID);
	if (strVar) {
		upper = (upper > strVar->GetLength()) ? strVar->GetLength() - 1 : upper;
		substring = strVar->SubString(lower, upper - lower + 1);
	}
}

AssignableSubstringArrayElementToken::AssignableSubstringArrayElementToken(UInt32 _id, const ArrayKey& _key, UInt32 lbound, UInt32 ubound)
	: AssignableSubstringToken(_id, lbound, ubound), key(_key)
{
	ArrayVar *arr = g_ArrayMap.Get(value.arrID);
	if (!arr) return;

	const char* pElemStr;
	if (arr->GetElementString(&key, &pElemStr))
	{
		std::string elemStr(pElemStr);
		upper = (upper > elemStr.length()) ? elemStr.length() - 1 : upper;
		substring = elemStr.substr(lbound, ubound - lbound + 1);
	}
}

bool AssignableSubstringStringVarToken::Assign(const char* str)
{
	StringVar* strVar = g_StringMap.Get(value.arrID);
	if (strVar) {
		UInt32 len = strVar->GetLength();
		if (lower <= upper && upper < len) {
			strVar->Erase(lower, upper-lower + 1);
			if (str) {
				strVar->Insert(str, lower);
				substring = strVar->String();
			}
			return true;
		}
	}
	return false;
}

bool AssignableSubstringArrayElementToken::Assign(const char* str)
{
	ArrayElement *elem = g_ArrayMap.GetElement(value.arrID, &key);
	const char* pElemStr;
	if (elem && elem->GetAsString(&pElemStr) && (lower <= upper) && (upper < StrLen(pElemStr)))
	{
		std::string elemStr(pElemStr);
		elemStr.erase(lower, upper - lower + 1);
		if (str)
		{
			elemStr.insert(lower, str);
			elem->SetString(elemStr.c_str());
			substring = elemStr;
		}
		return true;
	}
	return false;
}

#endif

SliceToken::SliceToken(Slice* _slice) : ScriptToken(kTokenType_Slice, Script::eVarType_Invalid, 0), slice(_slice)
{
	//
}

Slice::Slice(const Slice* _slice)
{
	bIsString = _slice->bIsString;
	if (bIsString)
	{
		m_upperStr = _slice->m_upperStr;
		m_lowerStr = _slice->m_lowerStr;
	}
	else
	{
		m_upper = _slice->m_upper;
		m_lower = _slice->m_lower;
	}
}

/*****************************************

	ScriptToken value getters

*****************************************/

const char* ScriptToken::GetString()
{
	static const char* empty = "";
	const char* result = NULL;

	if (type == kTokenType_String)
		result = value.str.c_str();
#if RUNTIME
	else if (type == kTokenType_StringVar)
	{
		ResolveVariable();
		if (!value.var)
		{
			return "";
		}
		StringVar* strVar = g_StringMap.Get(value.var->data);
		if (strVar) result = strVar->GetCString();
		else result = NULL;
	}
	else if (type == kTokenType_Command)
	{
		auto* token = context->ExecuteCommandToken(this);
		if (token)
		{
			value.str = std::move(token->value.str);
			delete token;
			return value.str.c_str();
		}
	}
#endif
	if (result)
		return result;
	return empty;
}

UInt32 ScriptToken::GetFormID()
{
	if (type == kTokenType_Form)
		return value.formID;
#if RUNTIME
	if (type == kTokenType_RefVar)
	{
		ResolveVariable();
		if (!value.var)
		{
			return 0;
		}
		return *reinterpret_cast<UInt64*>(&value.var->data);
	}
	if (type == kTokenType_Command)
	{
		auto* token = context->ExecuteCommandToken(this);
		if (token)
		{
			const auto result = token->GetFormID();
			delete token;
			return result;
		}
		
	}
#endif
	else if (type == kTokenType_Number)
		return value.formID;
	else if (type == kTokenType_Ref && value.refVar)
		return value.refVar->form ? value.refVar->form->refID : 0;
	return 0;
}

TESForm* ScriptToken::GetTESForm()
{
	// ###TODO: handle Ref (RefVariable)? Read() turns RefVariable into Form so that type is compile-time only
#if RUNTIME
	if (type == kTokenType_Form)
		return LookupFormByID(value.formID);
	if (type == kTokenType_RefVar && value.var)
	{
		ResolveVariable();
		if (!value.var)
		{
			return nullptr;
		}
		return LookupFormByID(*reinterpret_cast<UInt64*>(&value.var->data));
	}
	if (type == kTokenType_Command)
	{
		auto* token = context->ExecuteCommandToken(this);
		if (token)
		{
			auto* result = token->GetTESForm();
			delete token;
			return result;
		}
	}
#endif

	if (type == kTokenType_Ref && value.refVar)
		return value.refVar->form;
	else
		return NULL;
}

double ScriptToken::GetNumber()
{
	if (type == kTokenType_Number || type == kTokenType_Boolean)
		return value.num;
	else if (type == kTokenType_Global && value.global)
		return value.global->data;
#if RUNTIME
	else if ((type == kTokenType_NumericVar && value.var) ||
	         (type == kTokenType_StringVar && value.var))
	{
		ResolveVariable();
		if (!value.var)
		{
			return 0.0;
		}
		return value.var->data;
	}
		
	else if (type == kTokenType_Command)
	{
		auto* token = context->ExecuteCommandToken(this);
		if (token)
		{
			const auto result = token->GetNumber();
			delete token;
			return result;
		}
	}
#endif
	return 0.0;
}

bool ScriptToken::GetBool()
{
	switch (type)
	{
	case kTokenType_Boolean:
	case kTokenType_Number:
		return value.num ? true : false;
	case kTokenType_Form:
		return value.formID ? true : false;
	case kTokenType_Global:
		return value.global->data ? true : false;
#if RUNTIME
	case kTokenType_Array:
		return value.arrID ? true : false;
	case kTokenType_NumericVar:
	case kTokenType_StringVar:
	case kTokenType_ArrayVar:
	case kTokenType_RefVar:
		{
			ResolveVariable();
			if (!value.var)
			{
				return false;
			}
			return value.var->data ? true : false;
		}
	case kTokenType_Command:
		{
			auto* token = context->ExecuteCommandToken(this);
			if (token)
			{
				const auto result = token->GetBool();
				delete token;
				return result;
			}
		}
#endif
	default:
		return false;
	}
}

Operator* ScriptToken::GetOperator() const
{
	return type == kTokenType_Operator ? value.op : NULL;
}

#if RUNTIME
ArrayID ScriptToken::GetArray()
{
	if (type == kTokenType_Array)
		return value.arrID;
	if (type == kTokenType_ArrayVar)
	{
		ResolveVariable();
		if (!value.var)
		{
			return false;
		}
		return value.var->data;
	}
	if (type == kTokenType_Command)
	{
		auto* token = context->ExecuteCommandToken(this);
		if (token)
		{
			const auto result = token->GetArray();
			delete token;
			return result;
		}
	}
	return 0;
}

ScriptEventList::Var* ScriptToken::GetVar()
{
	ResolveVariable();
	return IsVariable() ? value.var : NULL;
}

void ScriptToken::ResolveVariable()
{
	auto clear = false;
	if (g_scriptEventListsDestroyed != eventListsDestroyedCount)
	{		
		eventListsDestroyedCount = g_scriptEventListsDestroyed;
		clear = true;
	}
	if (value.var == nullptr || varIdx != value.var->id || this->scriptEventList != context->eventList || clear)
	{
		scriptEventList = context->eventList;
		if (refIdx)
		{
			Script::RefVariable* refVar = context->script->GetVariable(refIdx);
			if (refVar)
			{
				refVar->Resolve(context->eventList);
				if (refVar->form)
					scriptEventList = EventListFromForm(refVar->form);
			}
		}
		value.var = scriptEventList->GetVariable(varIdx);
	}
}
#endif

TESGlobal* ScriptToken::GetGlobal() const
{
	if (type == kTokenType_Global) return value.global;
	return NULL;
}

VariableInfo* ScriptToken::GetVarInfo() const
{
	switch (type) {
		case kTokenType_Variable:
		case kTokenType_StringVar:
		case kTokenType_ArrayVar:
		case kTokenType_NumericVar:
		case kTokenType_RefVar:
			return value.varInfo;
	}

	return NULL;
}

CommandInfo* ScriptToken::GetCommandInfo() const
{
	if (type == kTokenType_Command) return value.cmd;
	return NULL;
}

#if RUNTIME


UInt32 ScriptToken::GetActorValue()
{
	UInt32 actorVal = eActorVal_NoActorValue;
	if (CanConvertTo(kTokenType_Number)) {
		int num = GetNumber();
		if (num >= 0 && num <= eActorVal_FalloutMax) {
			actorVal = num;
		}
	}
	else {
		const char* str = GetString();
		if (str) {
			actorVal = GetActorValueForString(str, true);
		}
	}

	return actorVal;
}

char ScriptToken::GetAxis()
{
	char axis = -1;
	const char* str = GetString();
	if (str && str[0] && str[1] == '\0') {
		switch (str[0]) {
			case 'x':
			case 'X':
				axis = 'X';
				break;
			case 'y':
			case 'Y':
				axis = 'Y';
				break;
			case 'z':
			case 'Z':
				axis = 'Z';
				break;
		}
	}

	return axis;
}

UInt32 ScriptToken::GetSex()
{
	const char* str = GetString();
	if (str) {
		if (StrEqualCI(str, "male")) {
			return 0;
		}
		else if (StrEqualCI(str, "female")) {
			return 1;
		}
	}

	return -1;
}

#endif // RUNTIME

/*************************************************
	
	ScriptToken methods

*************************************************/

Token_Type ToTokenType(CommandReturnType type)
{
	switch (type) {
	case kRetnType_Default:
		return kTokenType_Number;
	case kRetnType_Form:
		return kTokenType_Form;
	case kRetnType_String:
		return kTokenType_String;
	case kRetnType_Array:
		return kTokenType_Array;
	default:
		return kTokenType_Invalid;
	}
}

bool ScriptToken::CanConvertTo(Token_Type to) const
{
	if (type == kTokenType_Command)
	{
		return CanConvertOperand(ToTokenType(returnType), to);
	}
	return CanConvertOperand(type, to);
}

#if RUNTIME

ScriptToken* ScriptToken::Read(ExpressionEvaluator* context)
{
	ScriptToken* newToken = new ScriptToken();
	if (newToken->ReadFrom(context) != kTokenType_Invalid)
		return newToken;

	delete newToken;
	return NULL;
}

Token_Type ScriptToken::ReadFrom(ExpressionEvaluator* context)
{
	UInt8 typeCode = context->ReadByte();
	this->owningScript = context->script;
	this->context = context;
	this->scriptEventList = context->eventList;
	switch (typeCode)
	{
	case 'B':
	case 'b':
		type = kTokenType_Number;
		value.num = context->ReadByte();
		break;
	case 'I':
	case 'i':
		type = kTokenType_Number;
		value.num = context->Read16();
		break;
	case 'L':
	case 'l':
		type = kTokenType_Number;
		value.num = context->Read32();
		break;
	case 'Z':
		type = kTokenType_Number;
		value.num = context->ReadFloat();
		break;
	case 'S':
	{
		type = kTokenType_String;
		UInt32 incData = 0;
		value.str = context->ReadString(incData);
		break;
	}
	case 'R':
		type = kTokenType_Ref;
		refIdx = context->Read16();
		value.refVar = context->script->GetVariable(refIdx);
		if (!value.refVar)
			type = kTokenType_Invalid;
		else
		{
			type = kTokenType_Form;
			value.refVar->Resolve(context->eventList);
			value.formID = value.refVar->form ? value.refVar->form->refID : 0;
		}
		//incrementData = 3;
		break;
	case 'G':
	{
		type = kTokenType_Global;
		refIdx = context->Read16();
		Script::RefVariable* refVar = context->script->GetVariable(refIdx);
		if (!refVar)
		{
			type = kTokenType_Invalid;
			break;
		}
		refVar->Resolve(context->eventList);
		value.global = DYNAMIC_CAST(refVar->form, TESForm, TESGlobal);
		if (!value.global)
			type = kTokenType_Invalid;

		break;
	}
	case 'X':
	{
		type = kTokenType_Command;
		refIdx = context->Read16();
		const auto opcode = context->Read16();
		value.cmd = g_scriptCommands.GetByOpcode(opcode);
		if (!value.cmd)
			type = kTokenType_Invalid;
		auto argsLen = context->Read16();
		opcodeOffset = context->m_data - context->m_scriptData;
		context->m_data += argsLen - 2;
		returnType = g_scriptCommands.GetReturnType(value.cmd);
		break;
	}
	case 'V':
	{
		variableType = context->ReadByte();
		switch (variableType)
		{
		case Script::eVarType_Array:
			type = kTokenType_ArrayVar;
			break;
		case Script::eVarType_Integer:
		case Script::eVarType_Float:
			type = kTokenType_NumericVar;
			break;
		case Script::eVarType_Ref:
			type = kTokenType_RefVar;
			break;
		case Script::eVarType_String:
			type = kTokenType_StringVar;
			break;
		default:
			type = kTokenType_Invalid;
		}

		refIdx = context->Read16();

		ScriptEventList* eventList = context->eventList;
		if (refIdx)
		{
			Script::RefVariable* refVar = context->script->GetVariable(refIdx);
			if (refVar)
			{
				refVar->Resolve(context->eventList);
				if (refVar->form)
					eventList = EventListFromForm(refVar->form);
			}
		}

		varIdx = context->Read16();
		value.var = NULL;
		if (eventList)
			value.var = eventList->GetVariable(varIdx);

		if (!value.var)
			type = kTokenType_Invalid;
		//incrementData = 6;
		break;
	}
	default:
	{
		if (typeCode < kOpType_Max)
		{
			type = kTokenType_Operator;
			value.op = &s_operators[typeCode];
		}
		else
		{
			context->Error("Unexpected token type %d (%02x) encountered", typeCode, typeCode);
			type = kTokenType_Invalid;
		}
		//incrementData = 1;
	}
	}
	return type;
}

#endif

// compiling typecodes to printable chars just makes verifying parser output much easier
static char TokenCodes[kTokenType_Max] =
{ 'Z', '!', 'S', '!', 'R', 'G', '!', '!', '!', 'X', 'V', 'V', 'V', 'V', 'V', '!', 'O', '!', 'B', 'I', 'L' };

STATIC_ASSERT(SIZEOF_ARRAY(TokenCodes, char) == kTokenType_Max);

inline char TokenTypeToCode(Token_Type type)
{
	return type < kTokenType_Max ? TokenCodes[type] : 0;
}

bool ScriptToken::Write(ScriptLineBuffer* buf)
{
	if (type != kTokenType_Operator && type != kTokenType_Number)
		buf->WriteByte(TokenTypeToCode(type));

	switch (type)
	{
	case kTokenType_Number:
		{
			if (floor(value.num) != value.num)
			{
				buf->WriteByte(TokenTypeToCode(kTokenType_Number));
				return buf->WriteFloat(value.num);
			}
			else		// unary- compiled as operator. all numeric values in scripts are positive.
			{
				UInt32 val = value.num;
				if (val < 0x100)
				{
					buf->WriteByte(TokenTypeToCode(kTokenType_Byte));
					return buf->WriteByte((UInt8)val);
				}
				else if (val < 0x10000)
				{
					buf->WriteByte(TokenTypeToCode(kTokenType_Short));
					return buf->Write16((UInt16)val);
				}
				else
				{
					buf->WriteByte(TokenTypeToCode(kTokenType_Int));
					return buf->Write32(val);
				}
			}
		}
	case kTokenType_String:
		return buf->WriteString(value.str.c_str());
	case kTokenType_Ref:
	case kTokenType_Global:
		return buf->Write16(refIdx);
	case kTokenType_Command:
		buf->Write16(refIdx);
		return buf->Write16(value.cmd->opcode);
	case kTokenType_NumericVar:
	case kTokenType_RefVar:
	case kTokenType_StringVar:
	case kTokenType_ArrayVar:
		buf->WriteByte(variableType);
		buf->Write16(refIdx);
		return buf->Write16(value.varInfo->idx);
	case kTokenType_Operator:
		return buf->WriteByte(value.op->type);

	// the rest are run-time only
	default:
		return false;
	}
}

#if RUNTIME
ScriptToken* ScriptToken::ToBasicToken()
{
	if (CanConvertTo(kTokenType_String))
		return Create(GetString());
	else if (CanConvertTo(kTokenType_Array))
		return CreateArray(GetArray());
	else if (CanConvertTo(kTokenType_Form))
		return CreateForm(GetFormID());
	else if (CanConvertTo(kTokenType_Number))
		return Create(GetNumber());
	else
		return NULL;
}

double ScriptToken::GetNumericRepresentation(bool bFromHex)
{
	double result = 0.0;

	if (CanConvertTo(kTokenType_Number))
		result = GetNumber();
	else if (CanConvertTo(kTokenType_String))
	{
		const char* str = GetString();

		if (!bFromHex)
		{
			// if string begins with "0x", interpret as hex
			Tokenizer tok(str, " \t\r\n");
			std::string pre;
			if (tok.NextToken(pre) != -1 && pre.length() >= 2 && StrEqualCI(pre.substr(0, 2).c_str(), "0x"))
				bFromHex = true;
		}

		if (!bFromHex)
			result = strtod(str, NULL);
		else
		{
			UInt32 hexInt = 0;
			sscanf_s(str, "%x", &hexInt);
			result = (double)hexInt;
		}
	}

	return result;
}

#endif

/****************************************
	
	ArrayElementToken

****************************************/

#if RUNTIME

bool ArrayElementToken::CanConvertTo(Token_Type to) const
{
	if (!IsGood())
		return false;
	else if (to == kTokenType_ArrayElement)
		return true;

	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (!arr) return false;

	DataType elemType = arr->GetElementType(&key);
	if (elemType == kDataType_Invalid)
		return false;

	switch (to)
	{
	case kTokenType_Boolean:
		return elemType == kDataType_Form || elemType == kDataType_Numeric;
	case kTokenType_String:
		return elemType == kDataType_String;
	case kTokenType_Number:
		return elemType == kDataType_Numeric;
	case kTokenType_Array:
		return elemType == kDataType_Array;
	case kTokenType_Form:
		return elemType == kDataType_Form;
	}

	return false;
}

SmallObjectsAllocator::Allocator<ArrayElementToken, 4> g_arrayTokenAllocator;

void* ArrayElementToken::operator new(size_t size)
{
	return g_arrayTokenAllocator.Allocate();
}

void ArrayElementToken::operator delete(void* p)
{
	g_arrayTokenAllocator.Free(p);
}

double ArrayElementToken::GetNumber()
{
	double out = 0.0;
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (arr) arr->GetElementNumber(&key, &out);
	return out;
}

const char* ArrayElementToken::GetString()
{
	const char* out = "";
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (arr) arr->GetElementString(&key, &out);
	return out;
}

UInt32 ArrayElementToken::GetFormID()
{
	UInt32 out = 0;
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (arr) arr->GetElementFormID(&key, &out);
	return out;
}

TESForm* ArrayElementToken::GetTESForm()
{
	TESForm* out = NULL;
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (arr) arr->GetElementForm(&key, &out);
	return out;
}

bool ArrayElementToken::GetBool()
{
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (!arr) return false;

	ArrayElement *elem = arr->Get(&key, false);
	if (!elem) return false;

	if (elem->DataType() == kDataType_Numeric)
		return elem->m_data.num != 0;
	else if (elem->DataType() == kDataType_Form)
		return elem->m_data.formID != 0;

	return false;
}

ArrayID ArrayElementToken::GetArray()
{
	ArrayID out = 0;
	ArrayVar *arr = g_ArrayMap.Get(GetOwningArrayID());
	if (arr) arr->GetElementArray(&key, &out);
	return out;
}

#endif

// Rules for converting from one operand type to another
Token_Type kConversions_Number[] =
{
	kTokenType_Boolean,
};

Token_Type kConversions_Boolean[] =
{
	kTokenType_Number,
};

Token_Type kConversions_Command[] =
{
#if !RUNTIME
	kTokenType_Ambiguous,
#endif

	kTokenType_Number,
	kTokenType_Form,
	kTokenType_Boolean,
};

Token_Type kConversions_Ref[] =
{
	kTokenType_Form,
	kTokenType_Boolean,
};

Token_Type kConversions_Global[] =
{
	kTokenType_Number,
	kTokenType_Boolean,
};

Token_Type kConversions_Form[] =
{
	kTokenType_Boolean,
};

Token_Type kConversions_NumericVar[] =
{
	kTokenType_Number,
	kTokenType_Boolean,
	kTokenType_Variable,
};

Token_Type kConversions_ArrayElement[] =
{
#if !RUNTIME
	kTokenType_Ambiguous,
#endif

	kTokenType_Number,
	kTokenType_Form,
	kTokenType_String,
	kTokenType_Array,
	kTokenType_Boolean,
};

Token_Type kConversions_RefVar[] =
{
	kTokenType_Form,
	kTokenType_Boolean,
	kTokenType_Variable,
};

Token_Type kConversions_StringVar[] =
{
	kTokenType_String,
	kTokenType_Variable,
	kTokenType_Boolean,
};

Token_Type kConversions_ArrayVar[] =
{
	kTokenType_Array,
	kTokenType_Variable,
	kTokenType_Boolean,
};

Token_Type kConversions_Array[] =
{
	kTokenType_Boolean,			// true if arrayID != 0, false if 0
};

Token_Type kConversions_AssignableString[] =
{
	kTokenType_String,
};

// just an array of the types to which a given token type can be converted
struct Operand
{
	Token_Type	* rules;
	UInt8		numRules;
};

// Operand definitions
#define OPERAND(x) kConversions_ ## x, SIZEOF_ARRAY(kConversions_ ## x, Token_Type)

static Operand s_operands[] =
{
	{	OPERAND(Number)		},
	{	OPERAND(Boolean)	},
	{	NULL,	0			},	// string has no conversions
	{	OPERAND(Form)		},
	{	OPERAND(Ref)		},
	{	OPERAND(Global)		},
	{	OPERAND(Array)		},
	{	OPERAND(ArrayElement)	},
	{	NULL,	0			},	// slice
	{	OPERAND(Command)	},
	{	NULL,	0			},	// variable
	{	OPERAND(NumericVar)	},
	{	OPERAND(RefVar)		},
	{	OPERAND(StringVar)	},
	{	OPERAND(ArrayVar)	},
	{	NULL,	0			},  // operator
	{	NULL,	0			},	// ambiguous
	{	NULL,	0			},	// forEachContext
	{	NULL,	0			},	// numeric placeholders, used only in bytecode
	{	NULL,	0			},
	{	NULL,	0			},
	{	NULL,	0			},	// pair
	{	OPERAND(AssignableString)	},
};

STATIC_ASSERT(SIZEOF_ARRAY(s_operands, Operand) == kTokenType_Max);

bool CanConvertOperand(Token_Type from, Token_Type to)
{
	if (from == to)
		return true;
	if (from >= kTokenType_Invalid || to >= kTokenType_Invalid)
		return false;

	Operand* op = &s_operands[from];
	for (UInt32 i = 0; i < op->numRules; i++)
	{
		auto& rule = op->rules[i];
		if (rule == to)
			return true;
	}

	return false;
}

// Operator
Token_Type Operator::GetResult(Token_Type lhs, Token_Type rhs)
{
	for (UInt32 i = 0; i < numRules; i++)
	{
		OperationRule* rule = &rules[i];
		if (CanConvertOperand(lhs, rule->lhs) && CanConvertOperand(rhs, rule->rhs))
			return rule->result;
		else if (!rule->bAsymmetric && CanConvertOperand(lhs, rule->rhs) && CanConvertOperand(rhs, rule->lhs))
			return rule->result;
	}

	return kTokenType_Invalid;
}

#if RUNTIME

void Slice::GetArrayBounds(ArrayKey& lo, ArrayKey& hi) const
{
	if (bIsString)
	{
		lo = ArrayKey(m_lowerStr.c_str());
		hi = ArrayKey(m_upperStr.c_str());
	}
	else
	{
		lo = ArrayKey(m_lower);
		hi = ArrayKey(m_upper);
	}
}

#endif