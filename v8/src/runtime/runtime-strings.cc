// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/regexp/jsregexp-inl.h"
#include "src/string-builder.h"
#include "src/string-search.h"
#include "src/bootstrapper.h"

namespace v8 {
    namespace internal {

	// This may return an empty MaybeHandle if an exception is thrown or
	// we abort due to reaching the recursion limit.
	MaybeHandle<String> StringReplaceOneCharWithString(Isolate* isolate, Handle<String> subject, Handle<String> search,
							   Handle<String> replace, bool* found, int recursion_limit){
	    StackLimitCheck stackLimitCheck(isolate);
	    if (stackLimitCheck.HasOverflowed() || (recursion_limit == 0)){
		return MaybeHandle<String>();
	    }
	    recursion_limit--;
	    if (subject->IsConsString()){
		ConsString* cons = ConsString::cast(*subject);
		Handle<String> first = Handle<String>(cons->first());
		Handle<String> second = Handle<String>(cons->second());
		Handle<String> new_first;
		if (!StringReplaceOneCharWithString(isolate, first, search, replace, found,
						    recursion_limit).ToHandle(&new_first)){
		    return MaybeHandle<String>();
		}
		if (*found) return isolate->factory()->NewConsString(new_first, second);

		Handle<String> new_second;
		if (!StringReplaceOneCharWithString(isolate, second, search, replace, found,
						    recursion_limit)
		    .ToHandle(&new_second)){
		    return MaybeHandle<String>();
		}
		if (*found) return isolate->factory()->NewConsString(first, new_second);

		return subject;
	    } else {
		int index = String::IndexOf(isolate, subject, search, 0);
		if (index == -1) return subject;
		*found = true;
		Handle<String> first = isolate->factory()->NewSubString(subject, 0, index);
		Handle<String> cons1;
		ASSIGN_RETURN_ON_EXCEPTION(
					   isolate, cons1, isolate->factory()->NewConsString(first, replace),
					   String);
		Handle<String> second =
		    isolate->factory()->NewSubString(subject, index + 1, subject->length());
		return isolate->factory()->NewConsString(cons1, second);
	    }
	}


	RUNTIME_FUNCTION(Runtime_StringReplaceOneCharWithString){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, search, 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, replace, 2);

	    // If the cons string tree is too deep, we simply abort the recursion and
	    // retry with a flattened subject string.
	    const int kRecursionLimit = 0x1000;
	    bool found = false;
	    Handle<String> result;
	    if (StringReplaceOneCharWithString(isolate, subject, search, replace, &found,
					       kRecursionLimit).ToHandle(&result)){
		return *result;
	    }
	    if (isolate->has_pending_exception()) return isolate->heap()->exception();

	    subject = String::Flatten(subject);
	    if (StringReplaceOneCharWithString(isolate, subject, search, replace, &found,
					       kRecursionLimit).ToHandle(&result)){
		return *result;
	    }
	    if (isolate->has_pending_exception()) return isolate->heap()->exception();
	    // In case of empty handle and no pending exception we have stack overflow.
	    return isolate->StackOverflow();
	}


	RUNTIME_FUNCTION(Runtime_StringIndexOf){
	    v8::internal::LookBackMap::statInsert("indexOf");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);
	    CONVERT_ARG_HANDLE_CHECKED(Object, index, 2);

	    uint32_t start_index = 0;
	    if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

	    CHECK(start_index <= static_cast<uint32_t>(sub->length()));
	    int position = String::IndexOf(isolate, sub, pat, start_index);
	    return Smi::FromInt(position);
	}
	
	RUNTIME_FUNCTION(Runtime_StringIncludes){
	    v8::internal::LookBackMap::statInsert("includes");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);
	    CONVERT_ARG_HANDLE_CHECKED(Object, index, 2);

	    uint32_t start_index = 0;
	    if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

	    CHECK(start_index <= static_cast<uint32_t>(sub->length()));
	    int position = String::IndexOf(isolate, sub, pat, start_index);
	    return Smi::FromInt(position);
	}

	template <typename schar, typename pchar>
	static int StringMatchBackwards(Vector<const schar> subject,
					Vector<const pchar> pattern, int idx){
	    int pattern_length = pattern.length();
	    DCHECK(pattern_length >= 1);
	    DCHECK(idx + pattern_length <= subject.length());

	    if (sizeof(schar) == 1 && sizeof(pchar) > 1){
		for (int i = 0; i < pattern_length; i++){
		    uc16 c = pattern[i];
		    if (c > String::kMaxOneByteCharCode){
			return -1;
		    }
		}
	    }

	    pchar pattern_first_char = pattern[0];
	    for (int i = idx; i >= 0; i--){
		if (subject[i] != pattern_first_char) continue;
		int j = 1;
		while (j < pattern_length){
		    if (pattern[j] != subject[i + j]){
			break;
		    }
		    j++;
		}
		if (j == pattern_length){
		    return i;
		}
	    }
	    return -1;
	}


	RUNTIME_FUNCTION(Runtime_StringLastIndexOf){
	    v8::internal::LookBackMap::statInsert("lastIndexOf");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);
	    CONVERT_ARG_HANDLE_CHECKED(Object, index, 2);

	    uint32_t start_index = 0;
	    if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

	    uint32_t pat_length = pat->length();
	    uint32_t sub_length = sub->length();

	    if (start_index + pat_length > sub_length){
		start_index = sub_length - pat_length;
	    }

	    if (pat_length == 0){
		return Smi::FromInt(start_index);
	    }

	    sub = String::Flatten(sub);
	    pat = String::Flatten(pat);

	    int position = -1;
	    DisallowHeapAllocation no_gc;  // ensure vectors stay valid

	    String::FlatContent sub_content = sub->GetFlatContent();
	    String::FlatContent pat_content = pat->GetFlatContent();

	    if (pat_content.IsOneByte()){
		Vector<const uint8_t> pat_vector = pat_content.ToOneByteVector();
		if (sub_content.IsOneByte()){
		    position = StringMatchBackwards(sub_content.ToOneByteVector(), pat_vector,
						    start_index);
		} else {
		    position = StringMatchBackwards(sub_content.ToUC16Vector(), pat_vector,
						    start_index);
		}
	    } else {
		Vector<const uc16> pat_vector = pat_content.ToUC16Vector();
		if (sub_content.IsOneByte()){
		    position = StringMatchBackwards(sub_content.ToOneByteVector(), pat_vector,
						    start_index);
		} else {
		    position = StringMatchBackwards(sub_content.ToUC16Vector(), pat_vector,
						    start_index);
		}
	    }

	    return Smi::FromInt(position);
	}


	RUNTIME_FUNCTION(Runtime_StringLocaleCompare){
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 2);

	    CONVERT_ARG_HANDLE_CHECKED(String, str1, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, str2, 1);

	    if (str1.is_identical_to(str2)) return Smi::FromInt(0);  // Equal.
	    
	    int str1_length = str1->length();
	    int str2_length = str2->length();

	    // Decide trivial cases without flattening.
	    if (str1_length == 0){
		if (str2_length == 0) return Smi::FromInt(0);  // Equal.
		return Smi::FromInt(-str2_length);
	    } else {
		if (str2_length == 0) return Smi::FromInt(str1_length);
	    }

	    int end = str1_length < str2_length ? str1_length : str2_length;

	    // No need to flatten if we are going to find the answer on the first
	    // character.  At this point we know there is at least one character
	    // in each string, due to the trivial case handling above.
	    int d = str1->Get(0) - str2->Get(0);
	    if (d != 0) return Smi::FromInt(d);

	    str1 = String::Flatten(str1);
	    str2 = String::Flatten(str2);

	    DisallowHeapAllocation no_gc;
	    String::FlatContent flat1 = str1->GetFlatContent();
	    String::FlatContent flat2 = str2->GetFlatContent();

	    for (int i = 0; i < end; i++){
		if (flat1.Get(i) != flat2.Get(i)){
		    return Smi::FromInt(flat1.Get(i) - flat2.Get(i));
		}
	    }

	    return Smi::FromInt(str1_length - str2_length);
	}


	RUNTIME_FUNCTION(Runtime_StringReplace){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 4);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    CONVERT_ARG_HANDLE_CHECKED(String, s, 3);
	    MaybeHandle<Object> resultMaybeObj = isolate->factory()->NewConsString(s,
										   isolate->factory()->NewSubString(string, start, end));
	    Handle<Object> resultObj;
	    resultMaybeObj.ToHandle(&resultObj);
	    Handle<Object> hsObj = string;

	    LookBackMap::append("replace", hsObj, resultObj);
	    
	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_SubString){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    Handle<Object> resultObj = isolate->factory()->NewSubString(string, start, end);
	    Handle<Object> hsObj = string;

	    LookBackMap::append("substring", hsObj, resultObj);

	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_StringSubStr){
	    v8::internal::LookBackMap::statInsert("substr");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    Handle<Object> resultObj = isolate->factory()->NewSubString(string, start, end);
	    Handle<Object> hsObj = string;

	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_StringSlice){
	    v8::internal::LookBackMap::statInsert("slice");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    Handle<Object> resultObj = isolate->factory()->NewSubString(string, start, end);
	    Handle<Object> hsObj = string;

	    return *resultObj;
	}
	
	RUNTIME_FUNCTION(Runtime_StringEndsWith){
	    v8::internal::LookBackMap::statInsert("endsWith");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    Handle<Object> resultObj = isolate->factory()->NewSubString(string, start, end);
	    Handle<Object> hsObj = string;

	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_StringStartsWith){
	    v8::internal::LookBackMap::statInsert("startsWith");

	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    int start, end;
	    // We have a fast integer-only case here to avoid a conversion to double in
	    // the common case where from and to are Smis.
	    if (args[1]->IsSmi() && args[2]->IsSmi()){
		CONVERT_SMI_ARG_CHECKED(from_number, 1);
		CONVERT_SMI_ARG_CHECKED(to_number, 2);
		start = from_number;
		end = to_number;
	    } else if (args[1]->IsNumber() && args[2]->IsNumber()){
		CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
		CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
		start = FastD2IChecked(from_number);
		end = FastD2IChecked(to_number);
	    } else {
		return isolate->ThrowIllegalOperation();
	    }
	    // The following condition is intentionally robust because the SubStringStub
	    // delegates here and we test this in cctest/test-strings/RobustSubStringStub.
	    if (end < start || start < 0 || end > string->length()){
		return isolate->ThrowIllegalOperation();
	    }
	    isolate->counters()->sub_string_runtime()->Increment();

	    Handle<Object> resultObj = isolate->factory()->NewSubString(string, start, end);
	    Handle<Object> hsObj = string;

	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_StringAdd){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 2);
	    CONVERT_ARG_HANDLE_CHECKED(Object, obj1, 0);
	    CONVERT_ARG_HANDLE_CHECKED(Object, obj2, 1);
	    isolate->counters()->string_add_runtime()->Increment();
	    MaybeHandle<String> maybe_str1(Object::ToString(isolate, obj1));
	    MaybeHandle<String> maybe_str2(Object::ToString(isolate, obj2));
	    Handle<String> str1;
	    Handle<String> str2;
	    maybe_str1.ToHandle(&str1);
	    maybe_str2.ToHandle(&str2);
	    RETURN_RESULT_OR_FAILURE(isolate,
				     isolate->factory()->NewConsString(str1, str2));
	}


	RUNTIME_FUNCTION(Runtime_InternalizeString){
	    HandleScope handles(isolate);
	    DCHECK(args.length() == 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
	    return *isolate->factory()->InternalizeString(string);
	}


	RUNTIME_FUNCTION(Runtime_StringMatch){
	    HandleScope handles(isolate);
	    DCHECK(args.length() == 3);

	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, regexp_info, 2);

	    CHECK(regexp_info->HasFastObjectElements());

	    RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
	    if (global_cache.HasException()) return isolate->heap()->exception();

	    int capture_count = regexp->CaptureCount();

	    ZoneScope zone_scope(isolate->runtime_zone());
	    ZoneList<int> offsets(8, zone_scope.zone());

	    while (true){
		int32_t* match = global_cache.FetchNext();
		if (match == NULL) break;
		offsets.Add(match[0], zone_scope.zone());  // start
		offsets.Add(match[1], zone_scope.zone());  // end
	    }

	    if (global_cache.HasException()) return isolate->heap()->exception();

	    if (offsets.length() == 0){
		// Not a single match.
		return isolate->heap()->null_value();
	    }

	    RegExpImpl::SetLastMatchInfo(regexp_info, subject, capture_count,
					 global_cache.LastSuccessfulMatch());

	    int matches = offsets.length() / 2;
	    Handle<FixedArray> elements = isolate->factory()->NewFixedArray(matches);
	    Handle<String> substring =
		isolate->factory()->NewSubString(subject, offsets.at(0), offsets.at(1));
	    elements->set(0, *substring);
	    FOR_WITH_HANDLE_SCOPE(isolate, int, i = 1, i, i < matches, i++, {
		    int from = offsets.at(i * 2);
		    int to = offsets.at(i * 2 + 1);
		    Handle<String> substring =
			isolate->factory()->NewProperSubString(subject, from, to);
		    elements->set(i, *substring);
		});
	    Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(elements);
	    result->set_length(Smi::FromInt(matches));
	    return *result;
	}


	RUNTIME_FUNCTION(Runtime_StringCharCodeAtRT){
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 2);
	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);
  
	    // Flatten the string.  If someone wants to get a char at an index
	    // in a cons string, it is likely that more indices will be
	    // accessed.
	    subject = String::Flatten(subject);

	    if (i >= static_cast<uint32_t>(subject->length())){
		return isolate->heap()->nan_value();
	    }

	    Handle<Object> obj = isolate->factory()->NewNumber(Smi::FromInt(subject->Get(i))->value(), NOT_TENURED, true);
	    return *obj;
	}


	RUNTIME_FUNCTION(Runtime_StringCompare){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    isolate->counters()->string_compare_runtime()->Increment();
	    switch (String::Compare(x, y)){
	    case ComparisonResult::kLessThan:
		return Smi::FromInt(LESS);
	    case ComparisonResult::kEqual:
		return Smi::FromInt(EQUAL);
	    case ComparisonResult::kGreaterThan:
		return Smi::FromInt(GREATER);
	    case ComparisonResult::kUndefined:
		break;
	    }
	    UNREACHABLE();
	    return Smi::FromInt(0);
	}


	RUNTIME_FUNCTION(Runtime_StringBuilderConcat){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
	    int32_t array_length;
	    if (!args[1]->ToInt32(&array_length)){
		THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
	    }
	    CONVERT_ARG_HANDLE_CHECKED(String, special, 2);

	    size_t actual_array_length = 0;
	    CHECK(TryNumberToSize(isolate, array->length(), &actual_array_length));
	    CHECK(array_length >= 0);
	    CHECK(static_cast<size_t>(array_length) <= actual_array_length);

	    // This assumption is used by the slice encoding in one or two smis.
	    DCHECK(Smi::kMaxValue >= String::kMaxLength);

	    CHECK(array->HasFastElements());
	    JSObject::EnsureCanContainHeapObjectElements(array);

	    int special_length = special->length();
	    if (!array->HasFastObjectElements()){
		return isolate->Throw(isolate->heap()->illegal_argument_string());
	    }

	    int length;
	    bool one_byte = special->HasOnlyOneByteChars();

	    {
		DisallowHeapAllocation no_gc;
		FixedArray* fixed_array = FixedArray::cast(array->elements());
		if (fixed_array->length() < array_length){
		    array_length = fixed_array->length();
		}

		if (array_length == 0){
		    return isolate->heap()->empty_string();
		} else if (array_length == 1){
		    Object* first = fixed_array->get(0);
		    if (first->IsString()) return first;
		}
		length = StringBuilderConcatLength(special_length, fixed_array,
						   array_length, &one_byte);
	    }

	    if (length == -1){
		return isolate->Throw(isolate->heap()->illegal_argument_string());
	    }

	    if (one_byte){
		Handle<SeqOneByteString> answer;
		ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						   isolate, answer, isolate->factory()->NewRawOneByteString(length));
		StringBuilderConcatHelper(*special, answer->GetChars(),
					  FixedArray::cast(array->elements()),
					  array_length);
		return *answer;
	    } else {
		Handle<SeqTwoByteString> answer;
		ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						   isolate, answer, isolate->factory()->NewRawTwoByteString(length));
		StringBuilderConcatHelper(*special, answer->GetChars(),
					  FixedArray::cast(array->elements()),
					  array_length);
		return *answer;
	    }
	}


	RUNTIME_FUNCTION(Runtime_StringBuilderJoin){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
	    int32_t array_length;
	    if (!args[1]->ToInt32(&array_length)){
		THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
	    }
	    CONVERT_ARG_HANDLE_CHECKED(String, separator, 2);
	    CHECK(array->HasFastObjectElements());
	    CHECK(array_length >= 0);

	    Handle<FixedArray> fixed_array(FixedArray::cast(array->elements()));
	    if (fixed_array->length() < array_length){
		array_length = fixed_array->length();
	    }

	    if (array_length == 0){
		return isolate->heap()->empty_string();
	    } else if (array_length == 1){
		Object* first = fixed_array->get(0);
		CHECK(first->IsString());
		return first;
	    }

	    int separator_length = separator->length();
	    CHECK(separator_length > 0);
	    int max_nof_separators =
		(String::kMaxLength + separator_length - 1) / separator_length;
	    if (max_nof_separators < (array_length - 1)){
		THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
	    }
	    int length = (array_length - 1) * separator_length;
	    for (int i = 0; i < array_length; i++){
		Object* element_obj = fixed_array->get(i);
		CHECK(element_obj->IsString());
		String* element = String::cast(element_obj);
		int increment = element->length();
		if (increment > String::kMaxLength - length){
		    STATIC_ASSERT(String::kMaxLength < kMaxInt);
		    length = kMaxInt;  // Provoke exception;
		    break;
		}
		length += increment;
	    }

	    Handle<SeqTwoByteString> answer;
	    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
					       isolate, answer, isolate->factory()->NewRawTwoByteString(length));

	    DisallowHeapAllocation no_gc;

	    uc16* sink = answer->GetChars();
#ifdef DEBUG
	    uc16* end = sink + length;
#endif

	    CHECK(fixed_array->get(0)->IsString());
	    String* first = String::cast(fixed_array->get(0));
	    String* separator_raw = *separator;

	    int first_length = first->length();
	    String::WriteToFlat(first, sink, 0, first_length);
	    sink += first_length;

	    for (int i = 1; i < array_length; i++){
		DCHECK(sink + separator_length <= end);
		String::WriteToFlat(separator_raw, sink, 0, separator_length);
		sink += separator_length;

		CHECK(fixed_array->get(i)->IsString());
		String* element = String::cast(fixed_array->get(i));
		int element_length = element->length();
		DCHECK(sink + element_length <= end);
		String::WriteToFlat(element, sink, 0, element_length);
		sink += element_length;
	    }
	    DCHECK(sink == end);

	    // Use %_FastOneByteArrayJoin instead.
	    DCHECK(!answer->IsOneByteRepresentation());
	    return *answer;
	}

	template <typename sinkchar>
	static void WriteRepeatToFlat(String* src, Vector<sinkchar> buffer, int cursor,
				      int repeat, int length){
	    if (repeat == 0) return;

	    sinkchar* start = &buffer[cursor];
	    String::WriteToFlat<sinkchar>(src, start, 0, length);

	    int done = 1;
	    sinkchar* next = start + length;

	    while (done < repeat){
		int block = Min(done, repeat - done);
		int block_chars = block * length;
		CopyChars(next, start, block_chars);
		next += block_chars;
		done += block;
	    }
	}

	template <typename Char>
	static void JoinSparseArrayWithSeparator(FixedArray* elements,
						 int elements_length,
						 uint32_t array_length,
						 String* separator,
						 Vector<Char> buffer){
	    DisallowHeapAllocation no_gc;
	    int previous_separator_position = 0;
	    int separator_length = separator->length();
	    DCHECK_LT(0, separator_length);
	    int cursor = 0;
	    for (int i = 0; i < elements_length; i += 2){
		int position = NumberToInt32(elements->get(i));
		String* string = String::cast(elements->get(i + 1));
		int string_length = string->length();
		if (string->length() > 0){
		    int repeat = position - previous_separator_position;
		    WriteRepeatToFlat<Char>(separator, buffer, cursor, repeat,
					    separator_length);
		    cursor += repeat * separator_length;
		    previous_separator_position = position;
		    String::WriteToFlat<Char>(string, &buffer[cursor], 0, string_length);
		    cursor += string->length();
		}
	    }

	    int last_array_index = static_cast<int>(array_length - 1);
	    // Array length must be representable as a signed 32-bit number,
	    // otherwise the total string length would have been too large.
	    DCHECK(array_length <= 0x7fffffff);  // Is int32_t.
	    int repeat = last_array_index - previous_separator_position;
	    WriteRepeatToFlat<Char>(separator, buffer, cursor, repeat, separator_length);
	    cursor += repeat * separator_length;
	    DCHECK(cursor <= buffer.length());
	}


	RUNTIME_FUNCTION(Runtime_SparseJoinWithSeparator){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, elements_array, 0);
	    CONVERT_NUMBER_CHECKED(uint32_t, array_length, Uint32, args[1]);
	    CONVERT_ARG_HANDLE_CHECKED(String, separator, 2);
	    // elements_array is fast-mode JSarray of alternating positions
	    // (increasing order) and strings.
	    CHECK(elements_array->HasFastSmiOrObjectElements());
	    // array_length is length of original array (used to add separators);
	    // separator is string to put between elements. Assumed to be non-empty.
	    CHECK(array_length > 0);

	    // Find total length of join result.
	    int string_length = 0;
	    bool is_one_byte = separator->IsOneByteRepresentation();
	    bool overflow = false;
	    CONVERT_NUMBER_CHECKED(int, elements_length, Int32, elements_array->length());
	    CHECK(elements_length <= elements_array->elements()->length());
	    CHECK((elements_length & 1) == 0);  // Even length.
	    FixedArray* elements = FixedArray::cast(elements_array->elements());
	    {
		DisallowHeapAllocation no_gc;
		for (int i = 0; i < elements_length; i += 2){
		    String* string = String::cast(elements->get(i + 1));
		    int length = string->length();
		    if (is_one_byte && !string->IsOneByteRepresentation()){
			is_one_byte = false;
		    }
		    if (length > String::kMaxLength ||
			String::kMaxLength - length < string_length){
			overflow = true;
			break;
		    }
		    string_length += length;
		}
	    }

	    int separator_length = separator->length();
	    if (!overflow && separator_length > 0){
		if (array_length <= 0x7fffffffu){
		    int separator_count = static_cast<int>(array_length) - 1;
		    int remaining_length = String::kMaxLength - string_length;
		    if ((remaining_length / separator_length) >= separator_count){
			string_length += separator_length * (array_length - 1);
		    } else {
			// Not room for the separators within the maximal string length.
			overflow = true;
		    }
		} else {
		    // Nonempty separator and at least 2^31-1 separators necessary
		    // means that the string is too large to create.
		    STATIC_ASSERT(String::kMaxLength < 0x7fffffff);
		    overflow = true;
		}
	    }
	    if (overflow){
		// Throw an exception if the resulting string is too large. See
		// https://code.google.com/p/chromium/issues/detail?id=336820
		// for details.
		THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewInvalidStringLengthError());
	    }

	    if (is_one_byte){
		Handle<SeqOneByteString> result = isolate->factory()
		    ->NewRawOneByteString(string_length)
		    .ToHandleChecked();
		JoinSparseArrayWithSeparator<uint8_t>(
						      FixedArray::cast(elements_array->elements()), elements_length,
						      array_length, *separator,
						      Vector<uint8_t>(result->GetChars(), string_length));
		return *result;
	    } else {
		Handle<SeqTwoByteString> result = isolate->factory()
		    ->NewRawTwoByteString(string_length)
		    .ToHandleChecked();
		JoinSparseArrayWithSeparator<uc16>(
						   FixedArray::cast(elements_array->elements()), elements_length,
						   array_length, *separator,
						   Vector<uc16>(result->GetChars(), string_length));
		return *result;
	    }
	}


	// Copies Latin1 characters to the given fixed array looking up
	// one-char strings in the cache. Gives up on the first char that is
	// not in the cache and fills the remainder with smi zeros. Returns
	// the length of the successfully copied prefix.
	static int CopyCachedOneByteCharsToArray(Heap* heap, const uint8_t* chars,
						 FixedArray* elements, int length){
	    DisallowHeapAllocation no_gc;
	    FixedArray* one_byte_cache = heap->single_character_string_cache();
	    Object* undefined = heap->undefined_value();
	    int i;
	    WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
	    for (i = 0; i < length; ++i){
		Object* value = one_byte_cache->get(chars[i]);
		if (value == undefined) break;
		elements->set(i, value, mode);
	    }
	    if (i < length){
		DCHECK(Smi::FromInt(0) == 0);
		memset(elements->data_start() + i, 0, kPointerSize * (length - i));
	    }
#ifdef DEBUG
	    for (int j = 0; j < length; ++j){
		Object* element = elements->get(j);
		DCHECK(element == Smi::FromInt(0) ||
		       (element->IsString() && String::cast(element)->LooksValid()));
	    }
#endif
	    return i;
	}


	// Converts a String to JSArray.
	// For example, "foo" => ["f", "o", "o"].
	RUNTIME_FUNCTION(Runtime_StringToArray){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 2);
	    CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
	    CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);

	    Handle<Object> hsObj = s;

	    s = String::Flatten(s);
	    const int length = static_cast<int>(Min<uint32_t>(s->length(), limit));

	    Handle<FixedArray> elements;
	    int position = 0;
	    if (s->IsFlat() && s->IsOneByteRepresentation()){
		// Try using cached chars where possible.
		elements = isolate->factory()->NewUninitializedFixedArray(length);

		DisallowHeapAllocation no_gc;
		String::FlatContent content = s->GetFlatContent();
		if (content.IsOneByte()){
		    Vector<const uint8_t> chars = content.ToOneByteVector();
		    // Note, this will initialize all elements (not only the prefix)
		    // to prevent GC from seeing partially initialized array.
		    position = CopyCachedOneByteCharsToArray(isolate->heap(), chars.start(),
							     *elements, length);
		} else {
		    MemsetPointer(elements->data_start(), isolate->heap()->undefined_value(),
				  length);
		}
	    } else {
		elements = isolate->factory()->NewFixedArray(length);
	    }
	    for (int i = position; i < length; ++i){
		Handle<Object> str =
		    isolate->factory()->LookupSingleCharacterStringFromCode(s->Get(i));
		elements->set(i, *str);
	    }

#ifdef DEBUG
	    for (int i = 0; i < length; ++i){
		DCHECK(String::cast(elements->get(i))->length() == 1);
	    }
#endif

	    for (int i = 0; i < elements->length(); i++){
		std::stringstream tstr;

		Handle<Object> resultObj = FixedArray::get(*elements, i, isolate);

		LookBackMap::append("split", hsObj, resultObj);
	    }

	    return *isolate->factory()->NewJSArrayWithElements(elements);
	}


	static inline bool ToUpperOverflows(uc32 character){
	    // y with umlauts and the micro sign are the only characters that stop
	    // fitting into one-byte when converting to uppercase.
	    static const uc32 yuml_code = 0xff;
	    static const uc32 micro_code = 0xb5;
	    return (character == yuml_code || character == micro_code);
	}


	template <class Converter>
	MUST_USE_RESULT static Object* ConvertCaseHelper(Isolate* isolate, String* string, SeqString* result, int result_length,
							 unibrow::Mapping<Converter, 128>* mapping){
	    DisallowHeapAllocation no_gc;
	    // We try this twice, once with the assumption that the result is no longer
	    // than the input and, if that assumption breaks, again with the exact
	    // length.  This may not be pretty, but it is nicer than what was here before
	    // and I hereby claim my vaffel-is.
	    //
	    // NOTE: This assumes that the upper/lower case of an ASCII
	    // character is also ASCII.  This is currently the case, but it
	    // might break in the future if we implement more context and locale
	    // dependent upper/lower conversions.
	    bool has_changed_character = false;

	    // Convert all characters to upper case, assuming that they will fit
	    // in the buffer
	    StringCharacterStream stream(string);
	    unibrow::uchar chars[Converter::kMaxWidth];
	    // We can assume that the string is not empty
	    uc32 current = stream.GetNext();
	    bool ignore_overflow = Converter::kIsToLower || result->IsSeqTwoByteString();
	    for (int i = 0; i < result_length;){
		bool has_next = stream.HasMore();
		uc32 next = has_next ? stream.GetNext() : 0;
		int char_length = mapping->get(current, next, chars);
		if (char_length == 0){
		    // The case conversion of this character is the character itself.
		    result->Set(i, current);
		    i++;
		} else if (char_length == 1 &&
			   (ignore_overflow || !ToUpperOverflows(current))){
		    // Common case: converting the letter resulted in one character.
		    DCHECK(static_cast<uc32>(chars[0]) != current);
		    result->Set(i, chars[0]);
		    has_changed_character = true;
		    i++;
		} else if (result_length == string->length()){
		    bool overflows = ToUpperOverflows(current);
		    // We've assumed that the result would be as long as the
		    // input but here is a character that converts to several
		    // characters.  No matter, we calculate the exact length
		    // of the result and try the whole thing again.
		    //
		    // Note that this leaves room for optimization.  We could just
		    // memcpy what we already have to the result string.  Also,
		    // the result string is the last object allocated we could
		    // "realloc" it and probably, in the vast majority of cases,
		    // extend the existing string to be able to hold the full
		    // result.
		    int next_length = 0;
		    if (has_next){
			next_length = mapping->get(next, 0, chars);
			if (next_length == 0) next_length = 1;
		    }
		    int current_length = i + char_length + next_length;
		    while (stream.HasMore()){
			current = stream.GetNext();
			overflows |= ToUpperOverflows(current);
			// NOTE: we use 0 as the next character here because, while
			// the next character may affect what a character converts to,
			// it does not in any case affect the length of what it convert
			// to.
			int char_length = mapping->get(current, 0, chars);
			if (char_length == 0) char_length = 1;
			current_length += char_length;
			if (current_length > String::kMaxLength){
			    AllowHeapAllocation allocate_error_and_return;
			    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
							   NewInvalidStringLengthError());
			}
		    }
		    // Try again with the real length.  Return signed if we need
		    // to allocate a two-byte string for to uppercase.
		    return (overflows && !ignore_overflow) ? Smi::FromInt(-current_length)
			: Smi::FromInt(current_length);
		} else {
		    for (int j = 0; j < char_length; j++){
			result->Set(i, chars[j]);
			i++;
		    }
		    has_changed_character = true;
		}
		current = next;
	    }
	    if (has_changed_character){
		return result;
	    } else {
		// If we didn't actually change anything in doing the conversion
		// we simple return the result and let the converted string
		// become garbage; there is no reason to keep two identical strings
		// alive.
		return string;
	    }
	}


	static const uintptr_t kOneInEveryByte = kUintptrAllBitsSet / 0xFF;
	static const uintptr_t kAsciiMask = kOneInEveryByte << 7;

	// Given a word and two range boundaries returns a word with high bit
	// set in every byte iff the corresponding input byte was strictly in
	// the range (m, n). All the other bits in the result are cleared.
	// This function is only useful when it can be inlined and the
	// boundaries are statically known.
	// Requires: all bytes in the input word and the boundaries must be
	// ASCII (less than 0x7F).
	static inline uintptr_t AsciiRangeMask(uintptr_t w, char m, char n){
	    // Use strict inequalities since in edge cases the function could be
	    // further simplified.
	    DCHECK(0 < m && m < n);
	    // Has high bit set in every w byte less than n.
	    uintptr_t tmp1 = kOneInEveryByte * (0x7F + n) - w;
	    // Has high bit set in every w byte greater than m.
	    uintptr_t tmp2 = w + kOneInEveryByte * (0x7F - m);
	    return (tmp1 & tmp2 & (kOneInEveryByte * 0x80));
	}


#ifdef DEBUG
	static bool CheckFastAsciiConvert(char* dst, const char* src, int length,
					  bool changed, bool is_to_lower){
	    bool expected_changed = false;
	    for (int i = 0; i < length; i++){
		if (dst[i] == src[i]) continue;
		expected_changed = true;
		if (is_to_lower){
		    DCHECK('A' <= src[i] && src[i] <= 'Z');
		    DCHECK(dst[i] == src[i] + ('a' - 'A'));
		} else {
		    DCHECK('a' <= src[i] && src[i] <= 'z');
		    DCHECK(dst[i] == src[i] - ('a' - 'A'));
		}
	    }
	    return (expected_changed == changed);
	}
#endif


	template <class Converter>
	static bool FastAsciiConvert(char* dst, const char* src, int length,
				     bool* changed_out){
#ifdef DEBUG
	    char* saved_dst = dst;
	    const char* saved_src = src;
#endif
	    DisallowHeapAllocation no_gc;
	    // We rely on the distance between upper and lower case letters
	    // being a known power of 2.
	    DCHECK('a' - 'A' == (1 << 5));
	    // Boundaries for the range of input characters than require conversion.
	    static const char lo = Converter::kIsToLower ? 'A' - 1 : 'a' - 1;
	    static const char hi = Converter::kIsToLower ? 'Z' + 1 : 'z' + 1;
	    bool changed = false;
	    uintptr_t or_acc = 0;
	    const char* const limit = src + length;

	    // dst is newly allocated and always aligned.
	    DCHECK(IsAligned(reinterpret_cast<intptr_t>(dst), sizeof(uintptr_t)));
	    // Only attempt processing one word at a time if src is also aligned.
	    if (IsAligned(reinterpret_cast<intptr_t>(src), sizeof(uintptr_t))){
		// Process the prefix of the input that requires no conversion one aligned
		// (machine) word at a time.
		while (src <= limit - sizeof(uintptr_t)){
		    const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
		    or_acc |= w;
		    if (AsciiRangeMask(w, lo, hi) != 0){
			changed = true;
			break;
		    }
		    *reinterpret_cast<uintptr_t*>(dst) = w;
		    src += sizeof(uintptr_t);
		    dst += sizeof(uintptr_t);
		}
		// Process the remainder of the input performing conversion when
		// required one word at a time.
		while (src <= limit - sizeof(uintptr_t)){
		    const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
		    or_acc |= w;
		    uintptr_t m = AsciiRangeMask(w, lo, hi);
		    // The mask has high (7th) bit set in every byte that needs
		    // conversion and we know that the distance between cases is
		    // 1 << 5.
		    *reinterpret_cast<uintptr_t*>(dst) = w ^ (m >> 2);
		    src += sizeof(uintptr_t);
		    dst += sizeof(uintptr_t);
		}
	    }
	    // Process the last few bytes of the input (or the whole input if
	    // unaligned access is not supported).
	    while (src < limit){
		char c = *src;
		or_acc |= c;
		if (lo < c && c < hi){
		    c ^= (1 << 5);
		    changed = true;
		}
		*dst = c;
		++src;
		++dst;
	    }

	    if ((or_acc & kAsciiMask) != 0) return false;

	    DCHECK(CheckFastAsciiConvert(saved_dst, saved_src, length, changed,
					 Converter::kIsToLower));

	    *changed_out = changed;
	    return true;
	}


	template <class Converter>
	MUST_USE_RESULT static Object* ConvertCase(Handle<String> s, Isolate* isolate,
						   unibrow::Mapping<Converter, 128>* mapping){
	    s = String::Flatten(s);
	    int length = s->length();
	    // Assume that the string is not empty; we need this assumption later
	    if (length == 0) return *s;

	    // Simpler handling of ASCII strings.
	    //
	    // NOTE: This assumes that the upper/lower case of an ASCII
	    // character is also ASCII.  This is currently the case, but it
	    // might break in the future if we implement more context and locale
	    // dependent upper/lower conversions.
	    if (s->IsOneByteRepresentationUnderneath()){
		// Same length as input.
		Handle<SeqOneByteString> result =
		    isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
		DisallowHeapAllocation no_gc;
		String::FlatContent flat_content = s->GetFlatContent();
		DCHECK(flat_content.IsFlat());
		bool has_changed_character = false;
		bool is_ascii = FastAsciiConvert<Converter>(
							    reinterpret_cast<char*>(result->GetChars()),
							    reinterpret_cast<const char*>(flat_content.ToOneByteVector().start()),
							    length, &has_changed_character);
		// If not ASCII, we discard the result and take the 2 byte path.
		if (is_ascii) return has_changed_character ? *result : *s;
	    }

	    Handle<SeqString> result;  // Same length as input.
	    if (s->IsOneByteRepresentation()){
		result = isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
	    } else {
		result = isolate->factory()->NewRawTwoByteString(length).ToHandleChecked();
	    }

	    Object* answer = ConvertCaseHelper(isolate, *s, *result, length, mapping);
	    if (answer->IsException(isolate) || answer->IsString()) return answer;

	    DCHECK(answer->IsSmi());
	    length = Smi::cast(answer)->value();
	    if (s->IsOneByteRepresentation() && length > 0){
		ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						   isolate, result, isolate->factory()->NewRawOneByteString(length));
	    } else {
		if (length < 0) length = -length;
		ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						   isolate, result, isolate->factory()->NewRawTwoByteString(length));
	    }
	    return ConvertCaseHelper(isolate, *s, *result, length, mapping);
	}


	RUNTIME_FUNCTION(Runtime_StringToLowerCase){
	    v8::internal::LookBackMap::statInsert("toLowerCase");

	    HandleScope scope(isolate);
	    DCHECK_EQ(args.length(), 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
	    return ConvertCase(s, isolate, isolate->runtime_state()->to_lower_mapping());
	}


	RUNTIME_FUNCTION(Runtime_StringToUpperCase){
	    v8::internal::LookBackMap::statInsert("toUpperCase");

	    HandleScope scope(isolate);
	    DCHECK_EQ(args.length(), 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
	    return ConvertCase(s, isolate, isolate->runtime_state()->to_upper_mapping());
	}

	RUNTIME_FUNCTION(Runtime_StringToLocaleLowerCase){
	    v8::internal::LookBackMap::statInsert("toLocaleLowerCase");

	    HandleScope scope(isolate);
	    DCHECK_EQ(args.length(), 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
	    return ConvertCase(s, isolate, isolate->runtime_state()->to_lower_mapping());
	}


	RUNTIME_FUNCTION(Runtime_StringToLocaleUpperCase){
	    v8::internal::LookBackMap::statInsert("toLocaleUpperCase");

	    HandleScope scope(isolate);
	    DCHECK_EQ(args.length(), 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
	    return ConvertCase(s, isolate, isolate->runtime_state()->to_upper_mapping());
	}

	RUNTIME_FUNCTION(Runtime_StringLessThan){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    switch (String::Compare(x, y)){
	    case ComparisonResult::kLessThan:
		return isolate->heap()->true_value();
	    case ComparisonResult::kEqual:
	    case ComparisonResult::kGreaterThan:
		return isolate->heap()->false_value();
	    case ComparisonResult::kUndefined:
		break;
	    }
	    UNREACHABLE();
	    return Smi::FromInt(0);
	}

	RUNTIME_FUNCTION(Runtime_StringLessThanOrEqual){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    switch (String::Compare(x, y)){
	    case ComparisonResult::kEqual:
	    case ComparisonResult::kLessThan:
		return isolate->heap()->true_value();
	    case ComparisonResult::kGreaterThan:
		return isolate->heap()->false_value();
	    case ComparisonResult::kUndefined:
		break;
	    }
	    UNREACHABLE();
	    return Smi::FromInt(0);
	}

	RUNTIME_FUNCTION(Runtime_StringGreaterThan){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    switch (String::Compare(x, y)){
	    case ComparisonResult::kGreaterThan:
		return isolate->heap()->true_value();
	    case ComparisonResult::kEqual:
	    case ComparisonResult::kLessThan:
		return isolate->heap()->false_value();
	    case ComparisonResult::kUndefined:
		break;
	    }
	    UNREACHABLE();
	    return Smi::FromInt(0);
	}

	RUNTIME_FUNCTION(Runtime_StringGreaterThanOrEqual){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    switch (String::Compare(x, y)){
	    case ComparisonResult::kEqual:
	    case ComparisonResult::kGreaterThan:
		return isolate->heap()->true_value();
	    case ComparisonResult::kLessThan:
		return isolate->heap()->false_value();
	    case ComparisonResult::kUndefined:
		break;
	    }
	    UNREACHABLE();
	    return Smi::FromInt(0);
	}

	RUNTIME_FUNCTION(Runtime_StringEqual){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    return isolate->heap()->ToBoolean(String::Equals(x, y));
	}

	RUNTIME_FUNCTION(Runtime_StringNotEqual){
	    HandleScope handle_scope(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, y, 1);
	    return isolate->heap()->ToBoolean(!String::Equals(x, y));
	}

	RUNTIME_FUNCTION(Runtime_FlattenString){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
	    String* result = *String::Flatten(str);
	    if (!isolate->bootstrapper()->IsActive()){
		std::stringstream target, from;
		from << *str;
		target << result;
		LookBackMap::assign(target.str(), from.str());
	    }
	    return result;
	}


	RUNTIME_FUNCTION(Runtime_StringCharFromCode){
	    HandleScope handlescope(isolate);
	    DCHECK_EQ(1, args.length());

	    if (args[0]->IsNumber()){
		CONVERT_NUMBER_CHECKED(uint32_t, code, Uint32, args[0]);
		code &= 0xffff;

		Handle<Object> hsObj = args.at<Object>(0);
		Handle<Object> resultObj = isolate->factory()->LookupSingleCharacterStringFromCode(code);
   
		LookBackMap::append("fromCharCode", hsObj, resultObj);
		return *resultObj;
	    }
	    return isolate->heap()->empty_string();
	}

	RUNTIME_FUNCTION(Runtime_ExternalStringGetChar){
	    SealHandleScope shs(isolate);
	    DCHECK_EQ(2, args.length());
	    CONVERT_ARG_CHECKED(ExternalString, string, 0);
	    CONVERT_INT32_ARG_CHECKED(index, 1);
	    return Smi::FromInt(string->Get(index));
	}
	/*
	  RUNTIME_FUNCTION(Runtime_OneByteSeqStringGetChar){
	  SealHandleScope shs(isolate);
	  DCHECK(args.length() == 2);
	  CONVERT_ARG_CHECKED(SeqOneByteString, string, 0);
	  CONVERT_INT32_ARG_CHECKED(index, 1);
	  return Smi::FromInt(string->SeqOneByteStringGet(index));
	  }


	  RUNTIME_FUNCTION(Runtime_OneByteSeqStringSetChar){
	  SealHandleScope shs(isolate);
	  DCHECK(args.length() == 3);
	  CONVERT_INT32_ARG_CHECKED(index, 0);
	  CONVERT_INT32_ARG_CHECKED(value, 1);
	  CONVERT_ARG_CHECKED(SeqOneByteString, string, 2);
	  string->SeqOneByteStringSet(index, value);
	  return string;
	  }


	  RUNTIME_FUNCTION(Runtime_TwoByteSeqStringGetChar){
	  SealHandleScope shs(isolate);
	  DCHECK(args.length() == 2);
	  CONVERT_ARG_CHECKED(SeqTwoByteString, string, 0);
	  CONVERT_INT32_ARG_CHECKED(index, 1);
	  return Smi::FromInt(string->SeqTwoByteStringGet(index));
	  }


	  RUNTIME_FUNCTION(Runtime_TwoByteSeqStringSetChar){
	  SealHandleScope shs(isolate);
	  DCHECK(args.length() == 3);
	  CONVERT_INT32_ARG_CHECKED(index, 0);
	  CONVERT_INT32_ARG_CHECKED(value, 1);
	  CONVERT_ARG_CHECKED(SeqTwoByteString, string, 2);
	  string->SeqTwoByteStringSet(index, value);
	  return string;
	  }
	*/
	RUNTIME_FUNCTION(Runtime_StringCharAt){
	    v8::internal::LookBackMap::statInsert("charAt");

	    SealHandleScope shs(isolate);
	    DCHECK(args.length() == 2);
	    if (!args[0]->IsString()) return Smi::FromInt(0);
	    if (!args[1]->IsNumber()) return Smi::FromInt(0);
	    if (std::isinf(args.number_at(1))) return isolate->heap()->empty_string();
	    Object* code = __RT_impl_Runtime_StringCharCodeAtRT(args, isolate);
	    if (code->IsNaN()) return isolate->heap()->empty_string();
	    return __RT_impl_Runtime_StringCharFromCode(Arguments(1, &code), isolate);
	}
	
	RUNTIME_FUNCTION(Runtime_StringCharCodeAt){
	    v8::internal::LookBackMap::statInsert("charCodeAt");

	    SealHandleScope shs(isolate);
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 2);
	    if (!args[0]->IsString()) return isolate->heap()->undefined_value();
	    if (!args[1]->IsNumber()) return isolate->heap()->undefined_value();
	    if (std::isinf(args.number_at(1))) return isolate->heap()->nan_value();

	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);
  
	    // Flatten the string.  If someone wants to get a char at an index
	    // in a cons string, it is likely that more indices will be
	    // accessed.
	    subject = String::Flatten(subject);

	    if (i >= static_cast<uint32_t>(subject->length())){
		return isolate->heap()->nan_value();
	    }

	    Handle<Object> resultObj = isolate->factory()->NewNumber(Smi::FromInt(subject->Get(i))->value(), NOT_TENURED, true);

	    // Handle<Object> result = __RT_impl_Runtime_StringCharCodeAtRT(args, isolate);
	    
	    if (!isolate->bootstrapper()->IsActive()){
		Handle<Object> hsObj = args.at<Object>(0);

		LookBackMap::append("charCodeAt", hsObj, resultObj);
	    } 
	    return *resultObj;
	}

	RUNTIME_FUNCTION(Runtime_StringCodePointAt){
	    v8::internal::LookBackMap::statInsert("codePointAt");

	    SealHandleScope shs(isolate);
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 2);
	    if (!args[0]->IsString()) return isolate->heap()->undefined_value();
	    if (!args[1]->IsNumber()) return isolate->heap()->undefined_value();
	    if (std::isinf(args.number_at(1))) return isolate->heap()->nan_value();

	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);
  
	    // Flatten the string.  If someone wants to get a char at an index
	    // in a cons string, it is likely that more indices will be
	    // accessed.
	    subject = String::Flatten(subject);

	    if (i >= static_cast<uint32_t>(subject->length())){
		return isolate->heap()->nan_value();
	    }

	    Handle<Object> resultObj = isolate->factory()->NewNumber(Smi::FromInt(subject->Get(i))->value(), NOT_TENURED, true);

	    return *resultObj;
	}

    }  // namespace internal
}  // namespace v8
