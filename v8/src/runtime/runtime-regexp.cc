// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/regexp/jsregexp-inl.h"
#include "src/regexp/jsregexp.h"
#include "src/string-builder.h"
#include "src/string-search.h"

namespace v8 {
    namespace internal {
	class CompiledReplacement {
	public:
	    explicit CompiledReplacement(Zone* zone)
		: parts_(1, zone), replacement_substrings_(0, zone), zone_(zone){}

	    // Return whether the replacement is simple.
	    bool Compile(Handle<String> replacement, int capture_count,
			 int subject_length);

	    // Use Apply only if Compile returned false.
	    void Apply(ReplacementStringBuilder* builder, int match_from, int match_to,
		       int32_t* match);

	    // Number of distinct parts of the replacement pattern.
	    int parts(){ return parts_.length(); }

	    Zone* zone() const { return zone_; }

	private:
	    enum PartType {
		SUBJECT_PREFIX = 1,
		SUBJECT_SUFFIX,
		SUBJECT_CAPTURE,
		REPLACEMENT_SUBSTRING,
		REPLACEMENT_STRING,
		NUMBER_OF_PART_TYPES
	    };

	    struct ReplacementPart {
		static inline ReplacementPart SubjectMatch(){
		    return ReplacementPart(SUBJECT_CAPTURE, 0);
		}
		static inline ReplacementPart SubjectCapture(int capture_index){
		    return ReplacementPart(SUBJECT_CAPTURE, capture_index);
		}
		static inline ReplacementPart SubjectPrefix(){
		    return ReplacementPart(SUBJECT_PREFIX, 0);
		}
		static inline ReplacementPart SubjectSuffix(int subject_length){
		    return ReplacementPart(SUBJECT_SUFFIX, subject_length);
		}
		static inline ReplacementPart ReplacementString(){
		    return ReplacementPart(REPLACEMENT_STRING, 0);
		}
		static inline ReplacementPart ReplacementSubString(int from, int to){
		    DCHECK(from >= 0);
		    DCHECK(to > from);
		    return ReplacementPart(-from, to);
		}

		// If tag <= 0 then it is the negation of a start index of a substring of
		// the replacement pattern, otherwise it's a value from PartType.
		ReplacementPart(int tag, int data) : tag(tag), data(data){
		    // Must be non-positive or a PartType value.
		    DCHECK(tag < NUMBER_OF_PART_TYPES);
		}
		// Either a value of PartType or a non-positive number that is
		// the negation of an index into the replacement string.
		int tag;
		// The data value's interpretation depends on the value of tag:
		// tag == SUBJECT_PREFIX ||
		// tag == SUBJECT_SUFFIX:  data is unused.
		// tag == SUBJECT_CAPTURE: data is the number of the capture.
		// tag == REPLACEMENT_SUBSTRING ||
		// tag == REPLACEMENT_STRING:    data is index into array of substrings
		//                               of the replacement string.
		// tag <= 0: Temporary representation of the substring of the replacement
		//           string ranging over -tag .. data.
		//           Is replaced by REPLACEMENT_{SUB,}STRING when we create the
		//           substring objects.
		int data;
	    };

	    template <typename Char>
	    bool ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
					 Vector<Char> characters, int capture_count,
					 int subject_length, Zone* zone){
		int length = characters.length();
		int last = 0;
		for (int i = 0; i < length; i++){
		    Char c = characters[i];
		    if (c == '$'){
			int next_index = i + 1;
			if (next_index == length){  // No next character!
			    break;
			}
			Char c2 = characters[next_index];
			switch (c2){
			case '$':
			    if (i > last){
				// There is a substring before. Include the first "$".
				parts->Add(
					   ReplacementPart::ReplacementSubString(last, next_index),
					   zone);
				last = next_index + 1;  // Continue after the second "$".
			    } else {
				// Let the next substring start with the second "$".
				last = next_index;
			    }
			    i = next_index;
			    break;
			case '`':
			    if (i > last){
				parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
			    }
			    parts->Add(ReplacementPart::SubjectPrefix(), zone);
			    i = next_index;
			    last = i + 1;
			    break;
			case '\'':
			    if (i > last){
				parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
			    }
			    parts->Add(ReplacementPart::SubjectSuffix(subject_length), zone);
			    i = next_index;
			    last = i + 1;
			    break;
			case '&':
			    if (i > last){
				parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
			    }
			    parts->Add(ReplacementPart::SubjectMatch(), zone);
			    i = next_index;
			    last = i + 1;
			    break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9': {
			    int capture_ref = c2 - '0';
			    if (capture_ref > capture_count){
				i = next_index;
				continue;
			    }
			    int second_digit_index = next_index + 1;
			    if (second_digit_index < length){
				// Peek ahead to see if we have two digits.
				Char c3 = characters[second_digit_index];
				if ('0' <= c3 && c3 <= '9'){  // Double digits.
				    int double_digit_ref = capture_ref * 10 + c3 - '0';
				    if (double_digit_ref <= capture_count){
					next_index = second_digit_index;
					capture_ref = double_digit_ref;
				    }
				}
			    }
			    if (capture_ref > 0){
				if (i > last){
				    parts->Add(ReplacementPart::ReplacementSubString(last, i),
					       zone);
				}
				DCHECK(capture_ref <= capture_count);
				parts->Add(ReplacementPart::SubjectCapture(capture_ref), zone);
				last = next_index + 1;
			    }
			    i = next_index;
			    break;
			}
			default:
			    i = next_index;
			    break;
			}
		    }
		}
		if (length > last){
		    if (last == 0){
			// Replacement is simple.  Do not use Apply to do the replacement.
			return true;
		    } else {
			parts->Add(ReplacementPart::ReplacementSubString(last, length), zone);
		    }
		}
		return false;
	    }

	    ZoneList<ReplacementPart> parts_;
	    ZoneList<Handle<String> > replacement_substrings_;
	    Zone* zone_;
	};


	bool CompiledReplacement::Compile(Handle<String> replacement, int capture_count,
					  int subject_length){
	    {
		DisallowHeapAllocation no_gc;
		String::FlatContent content = replacement->GetFlatContent();
		DCHECK(content.IsFlat());
		bool simple = false;
		if (content.IsOneByte()){
		    simple = ParseReplacementPattern(&parts_, content.ToOneByteVector(),
						     capture_count, subject_length, zone());
		} else {
		    DCHECK(content.IsTwoByte());
		    simple = ParseReplacementPattern(&parts_, content.ToUC16Vector(),
						     capture_count, subject_length, zone());
		}
		if (simple) return true;
	    }

	    Isolate* isolate = replacement->GetIsolate();
	    // Find substrings of replacement string and create them as String objects.
	    int substring_index = 0;
	    for (int i = 0, n = parts_.length(); i < n; i++){
		int tag = parts_[i].tag;
		if (tag <= 0){  // A replacement string slice.
		    int from = -tag;
		    int to = parts_[i].data;
		    replacement_substrings_.Add(
						isolate->factory()->NewSubString(replacement, from, to), zone());
		    parts_[i].tag = REPLACEMENT_SUBSTRING;
		    parts_[i].data = substring_index;
		    substring_index++;
		} else if (tag == REPLACEMENT_STRING){
		    replacement_substrings_.Add(replacement, zone());
		    parts_[i].data = substring_index;
		    substring_index++;
		}
	    }
	    return false;
	}


	void CompiledReplacement::Apply(ReplacementStringBuilder* builder,
					int match_from, int match_to, int32_t* match){
	    DCHECK_LT(0, parts_.length());
	    for (int i = 0, n = parts_.length(); i < n; i++){
		ReplacementPart part = parts_[i];
		switch (part.tag){
		case SUBJECT_PREFIX:
		    if (match_from > 0) builder->AddSubjectSlice(0, match_from);
		    break;
		case SUBJECT_SUFFIX: {
		    int subject_length = part.data;
		    if (match_to < subject_length){
			builder->AddSubjectSlice(match_to, subject_length);
		    }
		    break;
		}
		case SUBJECT_CAPTURE: {
		    int capture = part.data;
		    int from = match[capture * 2];
		    int to = match[capture * 2 + 1];
		    if (from >= 0 && to > from){
			builder->AddSubjectSlice(from, to);
		    }
		    break;
		}
		case REPLACEMENT_SUBSTRING:
		case REPLACEMENT_STRING:
		    builder->AddString(replacement_substrings_[part.data]);
		    break;
		default:
		    UNREACHABLE();
		}
	    }
	}


	void FindOneByteStringIndices(Vector<const uint8_t> subject, uint8_t pattern,
				      ZoneList<int>* indices, unsigned int limit,
				      Zone* zone){
	    DCHECK(limit > 0);
	    // Collect indices of pattern in subject using memchr.
	    // Stop after finding at most limit values.
	    const uint8_t* subject_start = subject.start();
	    const uint8_t* subject_end = subject_start + subject.length();
	    const uint8_t* pos = subject_start;
	    while (limit > 0){
		pos = reinterpret_cast<const uint8_t*>(
						       memchr(pos, pattern, subject_end - pos));
		if (pos == NULL) return;
		indices->Add(static_cast<int>(pos - subject_start), zone);
		pos++;
		limit--;
	    }
	}


	void FindTwoByteStringIndices(const Vector<const uc16> subject, uc16 pattern,
				      ZoneList<int>* indices, unsigned int limit,
				      Zone* zone){
	    DCHECK(limit > 0);
	    const uc16* subject_start = subject.start();
	    const uc16* subject_end = subject_start + subject.length();
	    for (const uc16* pos = subject_start; pos < subject_end && limit > 0; pos++){
		if (*pos == pattern){
		    indices->Add(static_cast<int>(pos - subject_start), zone);
		    limit--;
		}
	    }
	}


	template <typename SubjectChar, typename PatternChar>
	void FindStringIndices(Isolate* isolate, Vector<const SubjectChar> subject,
			       Vector<const PatternChar> pattern,
			       ZoneList<int>* indices, unsigned int limit, Zone* zone){
	    DCHECK(limit > 0);
	    // Collect indices of pattern in subject.
	    // Stop after finding at most limit values.
	    int pattern_length = pattern.length();
	    int index = 0;
	    StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
	    while (limit > 0){
		index = search.Search(subject, index);
		if (index < 0) return;
		indices->Add(index, zone);
		index += pattern_length;
		limit--;
	    }
	}


	void FindStringIndicesDispatch(Isolate* isolate, String* subject,
				       String* pattern, ZoneList<int>* indices,
				       unsigned int limit, Zone* zone){
	    {
		DisallowHeapAllocation no_gc;
		String::FlatContent subject_content = subject->GetFlatContent();
		String::FlatContent pattern_content = pattern->GetFlatContent();
		DCHECK(subject_content.IsFlat());
		DCHECK(pattern_content.IsFlat());
		if (subject_content.IsOneByte()){
		    Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
		    if (pattern_content.IsOneByte()){
			Vector<const uint8_t> pattern_vector =
			    pattern_content.ToOneByteVector();
			if (pattern_vector.length() == 1){
			    FindOneByteStringIndices(subject_vector, pattern_vector[0], indices,
						     limit, zone);
			} else {
			    FindStringIndices(isolate, subject_vector, pattern_vector, indices,
					      limit, zone);
			}
		    } else {
			FindStringIndices(isolate, subject_vector,
					  pattern_content.ToUC16Vector(), indices, limit, zone);
		    }
		} else {
		    Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
		    if (pattern_content.IsOneByte()){
			Vector<const uint8_t> pattern_vector =
			    pattern_content.ToOneByteVector();
			if (pattern_vector.length() == 1){
			    FindTwoByteStringIndices(subject_vector, pattern_vector[0], indices,
						     limit, zone);
			} else {
			    FindStringIndices(isolate, subject_vector, pattern_vector, indices,
					      limit, zone);
			}
		    } else {
			Vector<const uc16> pattern_vector = pattern_content.ToUC16Vector();
			if (pattern_vector.length() == 1){
			    FindTwoByteStringIndices(subject_vector, pattern_vector[0], indices,
						     limit, zone);
			} else {
			    FindStringIndices(isolate, subject_vector, pattern_vector, indices,
					      limit, zone);
			}
		    }
		}
	    }
	}


	template <typename ResultSeqString>
	MUST_USE_RESULT static Object* StringReplaceGlobalAtomRegExpWithString(Isolate* isolate, Handle<String> subject, Handle<JSRegExp> pattern_regexp,
									       Handle<String> replacement, Handle<JSArray> last_match_info){
	    DCHECK(subject->IsFlat());
	    DCHECK(replacement->IsFlat());

	    ZoneScope zone_scope(isolate->runtime_zone());
	    ZoneList<int> indices(8, zone_scope.zone());
	    DCHECK_EQ(JSRegExp::ATOM, pattern_regexp->TypeTag());
	    String* pattern =
		String::cast(pattern_regexp->DataAt(JSRegExp::kAtomPatternIndex));
	    int subject_len = subject->length();
	    int pattern_len = pattern->length();
	    int replacement_len = replacement->length();

	    FindStringIndicesDispatch(isolate, *subject, pattern, &indices, 0xffffffff,
				      zone_scope.zone());

	    int matches = indices.length();
	    if (matches == 0) return *subject;

	    // Detect integer overflow.
	    int64_t result_len_64 = (static_cast<int64_t>(replacement_len) -
				     static_cast<int64_t>(pattern_len)) *
		static_cast<int64_t>(matches) +
		static_cast<int64_t>(subject_len);
	    int result_len;
	    if (result_len_64 > static_cast<int64_t>(String::kMaxLength)){
		STATIC_ASSERT(String::kMaxLength < kMaxInt);
		result_len = kMaxInt;  // Provoke exception.
	    } else {
		result_len = static_cast<int>(result_len_64);
	    }

	    int subject_pos = 0;
	    int result_pos = 0;

	    MaybeHandle<SeqString> maybe_res;
	    if (ResultSeqString::kHasOneByteEncoding){
		maybe_res = isolate->factory()->NewRawOneByteString(result_len);
	    } else {
		maybe_res = isolate->factory()->NewRawTwoByteString(result_len);
	    }
	    Handle<SeqString> untyped_res;
	    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, untyped_res, maybe_res);
	    Handle<ResultSeqString> result = Handle<ResultSeqString>::cast(untyped_res);

	    for (int i = 0; i < matches; i++){
		// Copy non-matched subject content.
		if (subject_pos < indices.at(i)){
		    String::WriteToFlat(*subject, result->GetChars() + result_pos,
					subject_pos, indices.at(i));
		    result_pos += indices.at(i) - subject_pos;
		}

		// Replace match.
		if (replacement_len > 0){
		    String::WriteToFlat(*replacement, result->GetChars() + result_pos, 0,
					replacement_len);
		    result_pos += replacement_len;
		}

		subject_pos = indices.at(i) + pattern_len;
	    }
	    // Add remaining subject content at the end.
	    if (subject_pos < subject_len){
		String::WriteToFlat(*subject, result->GetChars() + result_pos, subject_pos,
				    subject_len);
	    }

	    int32_t match_indices[] = {indices.at(matches - 1),
				       indices.at(matches - 1) + pattern_len};
	    RegExpImpl::SetLastMatchInfo(last_match_info, subject, 0, match_indices);

	    LookBackMap::append("replace", subject, result);
	    
	    return *result;
	}


	MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithString(Isolate* isolate, Handle<String> subject, Handle<JSRegExp> regexp,
									   Handle<String> replacement, Handle<JSArray> last_match_info){
	    DCHECK(subject->IsFlat());
	    DCHECK(replacement->IsFlat());

	    int capture_count = regexp->CaptureCount();
	    int subject_length = subject->length();

	    // CompiledReplacement uses zone allocation.
	    ZoneScope zone_scope(isolate->runtime_zone());
	    CompiledReplacement compiled_replacement(zone_scope.zone());
	    bool simple_replace =
		compiled_replacement.Compile(replacement, capture_count, subject_length);

	    // Shortcut for simple non-regexp global replacements
	    if (regexp->TypeTag() == JSRegExp::ATOM && simple_replace){
		if (subject->HasOnlyOneByteChars() && replacement->HasOnlyOneByteChars()){
		    return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(isolate, subject, regexp, replacement, last_match_info);
		} else {
		    return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(isolate, subject, regexp, replacement, last_match_info);
		}
	    }

	    RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
	    if (global_cache.HasException()) return isolate->heap()->exception();

	    int32_t* current_match = global_cache.FetchNext();
	    if (current_match == NULL){
		if (global_cache.HasException()) return isolate->heap()->exception();
		return *subject;
	    }

	    // Guessing the number of parts that the final result string is built
	    // from. Global regexps can match any number of times, so we guess
	    // conservatively.
	    int expected_parts = (compiled_replacement.parts() + 1) * 4 + 1;
	    ReplacementStringBuilder builder(isolate->heap(), subject, expected_parts);

	    // Number of parts added by compiled replacement plus preceeding
	    // string and possibly suffix after last match.  It is possible for
	    // all components to use two elements when encoded as two smis.
	    const int parts_added_per_loop = 2 * (compiled_replacement.parts() + 2);

	    int prev = 0;

	    do {
		builder.EnsureCapacity(parts_added_per_loop);

		int start = current_match[0];
		int end = current_match[1];

		if (prev < start){
		    builder.AddSubjectSlice(prev, start);
		}

		if (simple_replace){
		    builder.AddString(replacement);
		} else {
		    compiled_replacement.Apply(&builder, start, end, current_match);
		}
		prev = end;

		current_match = global_cache.FetchNext();
	    } while (current_match != NULL);

	    if (global_cache.HasException()) return isolate->heap()->exception();

	    if (prev < subject_length){
		builder.EnsureCapacity(2);
		builder.AddSubjectSlice(prev, subject_length);
	    }

	    RegExpImpl::SetLastMatchInfo(last_match_info, subject, capture_count,
					 global_cache.LastSuccessfulMatch());

	    RETURN_RESULT_OR_FAILURE(isolate, builder.ToString());
	    Handle<Object> result;
	    if (!(builder.ToString()).ToHandle(&result)){
		DCHECK(isolate->has_pending_exception());
		return isolate->heap()->exception();
	    }

	    LookBackMap::append("replace", subject, result);
	    
	    return *result;
	}
	

	template <typename ResultSeqString>
	MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithEmptyString(Isolate* isolate, Handle<String> subject, Handle<JSRegExp> regexp,
										Handle<JSArray> last_match_info){
	    DCHECK(subject->IsFlat());

	    // Shortcut for simple non-regexp global replacements
	    if (regexp->TypeTag() == JSRegExp::ATOM){
		Handle<String> empty_string = isolate->factory()->empty_string();
		if (subject->IsOneByteRepresentation()){
		    return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(isolate, subject, regexp, empty_string, last_match_info);
		} else {
		    return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(isolate, subject, regexp, empty_string, last_match_info);
		}
	    }

	    RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
	    if (global_cache.HasException()) return isolate->heap()->exception();

	    int32_t* current_match = global_cache.FetchNext();
	    if (current_match == NULL){
		if (global_cache.HasException()) return isolate->heap()->exception();
		return *subject;
	    }

	    int start = current_match[0];
	    int end = current_match[1];
	    int capture_count = regexp->CaptureCount();
	    int subject_length = subject->length();

	    int new_length = subject_length - (end - start);
	    if (new_length == 0) return isolate->heap()->empty_string();

	    Handle<ResultSeqString> answer;
	    if (ResultSeqString::kHasOneByteEncoding){
		answer = Handle<ResultSeqString>::cast(isolate->factory()->NewRawOneByteString(new_length).ToHandleChecked());
	    }
	    else{
		answer = Handle<ResultSeqString>::cast(isolate->factory()->NewRawTwoByteString(new_length).ToHandleChecked());
	    }

	    int prev = 0;
	    int position = 0;

	    do {
		start = current_match[0];
		end = current_match[1];
		if (prev < start){
		    // Add substring subject[prev;start] to answer string.
		    String::WriteToFlat(*subject, answer->GetChars() + position, prev, start);
		    position += start - prev;
		}
		prev = end;

		current_match = global_cache.FetchNext();
	    } while (current_match != NULL);

	    if (global_cache.HasException()) return isolate->heap()->exception();

	    RegExpImpl::SetLastMatchInfo(last_match_info, subject, capture_count,
					 global_cache.LastSuccessfulMatch());

	    if (prev < subject_length){
		// Add substring subject[prev;length] to answer string.
		String::WriteToFlat(*subject, answer->GetChars() + position, prev,
				    subject_length);
		position += subject_length - prev;
	    }

	    if (position == 0) return isolate->heap()->empty_string();

	    // Shorten string and fill
	    int string_size = ResultSeqString::SizeFor(position);
	    int allocated_string_size = ResultSeqString::SizeFor(new_length);
	    int delta = allocated_string_size - string_size;

	    answer->set_length(position);
	    if (delta == 0){
		LookBackMap::append("replace", subject, answer);
		return *answer;
	    }

	    Address end_of_string = answer->address() + string_size;
	    Heap* heap = isolate->heap();

	    // The trimming is performed on a newly allocated object, which is on a
	    // fresly allocated page or on an already swept page. Hence, the sweeper
	    // thread can not get confused with the filler creation. No synchronization
	    // needed.
	    // TODO(hpayer): We should shrink the large object page if the size
	    // of the object changed significantly.
	    if (!heap->lo_space()->Contains(*answer)){
		heap->CreateFillerObjectAt(end_of_string, delta, ClearRecordedSlots::kNo);
	    }
	    heap->AdjustLiveBytes(*answer, -delta, Heap::CONCURRENT_TO_SWEEPER);
	    LookBackMap::append("replace", subject, answer);
	    return *answer;
	}

	// jstrace todo
	RUNTIME_FUNCTION(Runtime_StringReplaceGlobalRegExpWithString){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 4);

	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, replacement, 2);
	    CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 3);

	    CHECK(regexp->GetFlags() & JSRegExp::kGlobal);
	    CHECK(last_match_info->HasFastObjectElements());

	    subject = String::Flatten(subject);

	    if (replacement->length() == 0){
		if (subject->HasOnlyOneByteChars()){
		    return StringReplaceGlobalRegExpWithEmptyString<SeqOneByteString>(isolate, subject, regexp, last_match_info);
		} else {
		    return StringReplaceGlobalRegExpWithEmptyString<SeqTwoByteString>(isolate, subject, regexp, last_match_info);
		}
	    }

	    replacement = String::Flatten(replacement);

	    return StringReplaceGlobalRegExpWithString(isolate, subject, regexp,
						       replacement, last_match_info);
	}


	RUNTIME_FUNCTION(Runtime_StringSplit){
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
	    CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);
	    CHECK(limit > 0);

	    Handle<Object> hsObj = subject;

	    int subject_length = subject->length();
	    int pattern_length = pattern->length();
	    CHECK(pattern_length > 0);

	    if (limit == 0xffffffffu){
		FixedArray* last_match_cache_unused;
		Handle<Object> cached_answer(RegExpResultsCache::Lookup(isolate->heap(), *subject, *pattern,
									&last_match_cache_unused,
									RegExpResultsCache::STRING_SPLIT_SUBSTRINGS),
					     isolate);
		if (*cached_answer != Smi::FromInt(0)){
		    // The cache FixedArray is a COW-array and can therefore be reused.
		    Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(Handle<FixedArray>::cast(cached_answer));
      
		    // try yacheng
		    Handle<FixedArray> fixedArray = Handle<FixedArray>::cast(cached_answer);
		    for (int i = 0; i < fixedArray->length(); i++){
			Handle<Object> resultObj = FixedArray::get(*fixedArray, i, isolate);

			LookBackMap::append("split", hsObj, resultObj);
		    } 

		    return *result;
		}
	    }

	    // The limit can be very large (0xffffffffu), but since the pattern
	    // isn't empty, we can never create more parts than ~half the length
	    // of the subject.

	    subject = String::Flatten(subject);
	    pattern = String::Flatten(pattern);

	    static const int kMaxInitialListCapacity = 16;

	    ZoneScope zone_scope(isolate->runtime_zone());

	    // Find (up to limit) indices of separator and end-of-string in subject
	    int initial_capacity = Min<uint32_t>(kMaxInitialListCapacity, limit);
	    ZoneList<int> indices(initial_capacity, zone_scope.zone());

	    FindStringIndicesDispatch(isolate, *subject, *pattern, &indices, limit,
				      zone_scope.zone());

	    if (static_cast<uint32_t>(indices.length()) < limit){
		indices.Add(subject_length, zone_scope.zone());
	    }

	    // The list indices now contains the end of each part to create.

	    // Create JSArray of substrings separated by separator.
	    int part_count = indices.length();

	    Handle<JSArray> result =
		isolate->factory()->NewJSArray(FAST_ELEMENTS, part_count, part_count,
					       INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);

	    DCHECK(result->HasFastObjectElements());

	    Handle<FixedArray> elements(FixedArray::cast(result->elements()));

	    if (part_count == 1 && indices.at(0) == subject_length){
		elements->set(0, *subject);
	    } else {
		int part_start = 0;
		FOR_WITH_HANDLE_SCOPE(isolate, int, i = 0, i, i < part_count, i++, {
			int part_end = indices.at(i);
			Handle<String> substring =
			    isolate->factory()->NewProperSubString(subject, part_start, part_end);
			elements->set(i, *substring);
			part_start = part_end + pattern_length;
		    });
	    }

	    if (limit == 0xffffffffu){
		if (result->HasFastObjectElements()){
		    RegExpResultsCache::Enter(isolate, subject, pattern, elements,
					      isolate->factory()->empty_fixed_array(),
					      RegExpResultsCache::STRING_SPLIT_SUBSTRINGS);
		}
	    }

  

	    // try yacheng
	    for (int i = 0; i < elements->length(); i++){
		Handle<Object> resultObj = FixedArray::get(*elements, i, isolate);
		LookBackMap::append("split", hsObj, resultObj);
	    } 

	    return *result;
	}


	RUNTIME_FUNCTION(Runtime_RegExpExec){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 4);
	    CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
	    CONVERT_INT32_ARG_CHECKED(index, 2);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 3);
	    // Due to the way the JS calls are constructed this must be less than the
	    // length of a string, i.e. it is always a Smi.  We check anyway for security.
	    CHECK(index >= 0);
	    CHECK(index <= subject->length());
	    isolate->counters()->regexp_entry_runtime()->Increment();
	    RETURN_RESULT_OR_FAILURE(
				     isolate, RegExpImpl::Exec(regexp, subject, index, last_match_info));
	}


	RUNTIME_FUNCTION(Runtime_RegExpFlags){
	    SealHandleScope shs(isolate);
	    DCHECK(args.length() == 1);
	    CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
	    return regexp->flags();
	}


	RUNTIME_FUNCTION(Runtime_RegExpSource){
	    SealHandleScope shs(isolate);
	    DCHECK(args.length() == 1);
	    CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
	    return regexp->source();
	}


	RUNTIME_FUNCTION(Runtime_RegExpConstructResult){
	    HandleScope handle_scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_SMI_ARG_CHECKED(size, 0);
	    CHECK(size >= 0 && size <= FixedArray::kMaxLength);
	    CONVERT_ARG_HANDLE_CHECKED(Object, index, 1);
	    CONVERT_ARG_HANDLE_CHECKED(Object, input, 2);
	    Handle<FixedArray> elements = isolate->factory()->NewFixedArray(size);
	    Handle<Map> regexp_map(isolate->native_context()->regexp_result_map());
	    Handle<JSObject> object =
		isolate->factory()->NewJSObjectFromMap(regexp_map, NOT_TENURED);
	    Handle<JSArray> array = Handle<JSArray>::cast(object);
	    array->set_elements(*elements);
	    array->set_length(Smi::FromInt(size));
	    // Write in-object properties after the length of the array.
	    array->InObjectPropertyAtPut(JSRegExpResult::kIndexIndex, *index);
	    array->InObjectPropertyAtPut(JSRegExpResult::kInputIndex, *input);
	    return *array;
	}


	RUNTIME_FUNCTION(Runtime_RegExpInitializeAndCompile){
	    HandleScope scope(isolate);
	    DCHECK(args.length() == 3);
	    CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
	    CONVERT_ARG_HANDLE_CHECKED(String, flags, 2);

	    RETURN_FAILURE_ON_EXCEPTION(isolate,
					JSRegExp::Initialize(regexp, source, flags));

	    return *regexp;
	}


	// Only called from Runtime_RegExpExecMultiple so it doesn't need to maintain
	// separate last match info.  See comment on that function.
	template <bool has_capture>
	static Object* SearchRegExpMultiple(Isolate* isolate, Handle<String> subject,
					    Handle<JSRegExp> regexp,
					    Handle<JSArray> last_match_array,
					    Handle<JSArray> result_array){
	    DCHECK(subject->IsFlat());
	    DCHECK_NE(has_capture, regexp->CaptureCount() == 0);

	    int capture_count = regexp->CaptureCount();
	    int subject_length = subject->length();

	    static const int kMinLengthToCache = 0x1000;

	    if (subject_length > kMinLengthToCache){
		FixedArray* last_match_cache;
		Object* cached_answer = RegExpResultsCache::Lookup(
								   isolate->heap(), *subject, regexp->data(), &last_match_cache,
								   RegExpResultsCache::REGEXP_MULTIPLE_INDICES);
		if (cached_answer->IsFixedArray()){
		    int capture_registers = (capture_count + 1) * 2;
		    int32_t* last_match = NewArray<int32_t>(capture_registers);
		    for (int i = 0; i < capture_registers; i++){
			last_match[i] = Smi::cast(last_match_cache->get(i))->value();
		    }
		    Handle<FixedArray> cached_fixed_array =
			Handle<FixedArray>(FixedArray::cast(cached_answer));
		    // The cache FixedArray is a COW-array and can therefore be reused.
		    JSArray::SetContent(result_array, cached_fixed_array);
		    RegExpImpl::SetLastMatchInfo(last_match_array, subject, capture_count,
						 last_match);
		    DeleteArray(last_match);
		    return *result_array;
		}
	    }

	    RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
	    if (global_cache.HasException()) return isolate->heap()->exception();

	    // Ensured in Runtime_RegExpExecMultiple.
	    DCHECK(result_array->HasFastObjectElements());
	    Handle<FixedArray> result_elements(
					       FixedArray::cast(result_array->elements()));
	    if (result_elements->length() < 16){
		result_elements = isolate->factory()->NewFixedArrayWithHoles(16);
	    }

	    FixedArrayBuilder builder(result_elements);

	    // Position to search from.
	    int match_start = -1;
	    int match_end = 0;
	    bool first = true;

	    // Two smis before and after the match, for very long strings.
	    static const int kMaxBuilderEntriesPerRegExpMatch = 5;

	    while (true){
		int32_t* current_match = global_cache.FetchNext();
		if (current_match == NULL) break;
		match_start = current_match[0];
		builder.EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
		if (match_end < match_start){
		    ReplacementStringBuilder::AddSubjectSlice(&builder, match_end,
							      match_start);
		}
		match_end = current_match[1];
		{
		    // Avoid accumulating new handles inside loop.
		    HandleScope temp_scope(isolate);
		    Handle<String> match;
		    if (!first){
			match = isolate->factory()->NewProperSubString(subject, match_start,
								       match_end);
		    } else {
			match =
			    isolate->factory()->NewSubString(subject, match_start, match_end);
			first = false;
		    }

		    if (has_capture){
			// Arguments array to replace function is match, captures, index and
			// subject, i.e., 3 + capture count in total.
			Handle<FixedArray> elements =
			    isolate->factory()->NewFixedArray(3 + capture_count);

			elements->set(0, *match);
			for (int i = 1; i <= capture_count; i++){
			    int start = current_match[i * 2];
			    if (start >= 0){
				int end = current_match[i * 2 + 1];
				DCHECK(start <= end);
				Handle<String> substring =
				    isolate->factory()->NewSubString(subject, start, end);
				elements->set(i, *substring);
			    } else {
				DCHECK(current_match[i * 2 + 1] < 0);
				elements->set(i, isolate->heap()->undefined_value());
			    }
			}
			elements->set(capture_count + 1, Smi::FromInt(match_start));
			elements->set(capture_count + 2, *subject);
			builder.Add(*isolate->factory()->NewJSArrayWithElements(elements));
		    } else {
			builder.Add(*match);
		    }
		}
	    }

	    if (global_cache.HasException()) return isolate->heap()->exception();

	    if (match_start >= 0){
		// Finished matching, with at least one match.
		if (match_end < subject_length){
		    ReplacementStringBuilder::AddSubjectSlice(&builder, match_end,
							      subject_length);
		}

		RegExpImpl::SetLastMatchInfo(last_match_array, subject, capture_count,
					     global_cache.LastSuccessfulMatch());

		if (subject_length > kMinLengthToCache){
		    // Store the last successful match into the array for caching.
		    // TODO(yangguo): do not expose last match to JS and simplify caching.
		    int capture_registers = (capture_count + 1) * 2;
		    Handle<FixedArray> last_match_cache =
			isolate->factory()->NewFixedArray(capture_registers);
		    int32_t* last_match = global_cache.LastSuccessfulMatch();
		    for (int i = 0; i < capture_registers; i++){
			last_match_cache->set(i, Smi::FromInt(last_match[i]));
		    }
		    Handle<FixedArray> result_fixed_array = builder.array();
		    result_fixed_array->Shrink(builder.length());
		    // Cache the result and turn the FixedArray into a COW array.
		    RegExpResultsCache::Enter(
					      isolate, subject, handle(regexp->data(), isolate), result_fixed_array,
					      last_match_cache, RegExpResultsCache::REGEXP_MULTIPLE_INDICES);
		}
		return *builder.ToJSArray(result_array);
	    } else {
		return isolate->heap()->null_value();  // No matches at all.
	    }
	}


	// This is only called for StringReplaceGlobalRegExpWithFunction.  This sets
	// lastMatchInfoOverride to maintain the last match info, so we don't need to
	// set any other last match array info.
	RUNTIME_FUNCTION(Runtime_RegExpExecMultiple){
	    HandleScope handles(isolate);
	    DCHECK(args.length() == 4);

	    CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
	    CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 2);
	    CONVERT_ARG_HANDLE_CHECKED(JSArray, result_array, 3);
	    CHECK(last_match_info->HasFastObjectElements());
	    CHECK(result_array->HasFastObjectElements());

	    subject = String::Flatten(subject);
	    CHECK(regexp->GetFlags() & JSRegExp::kGlobal);

	    if (regexp->CaptureCount() == 0){
		return SearchRegExpMultiple<false>(isolate, subject, regexp,
						   last_match_info, result_array);
	    } else {
		return SearchRegExpMultiple<true>(isolate, subject, regexp, last_match_info,
						  result_array);
	    }
	}


	RUNTIME_FUNCTION(Runtime_RegExpExecReThrow){
	    SealHandleScope shs(isolate);
	    DCHECK(args.length() == 4);
	    Object* exception = isolate->pending_exception();
	    isolate->clear_pending_exception();
	    return isolate->ReThrow(exception);
	}


	RUNTIME_FUNCTION(Runtime_IsRegExp){
	    SealHandleScope shs(isolate);
	    DCHECK(args.length() == 1);
	    CONVERT_ARG_CHECKED(Object, obj, 0);
	    return isolate->heap()->ToBoolean(obj->IsJSRegExp());
	}
    }  // namespace internal
}  // namespace v8
