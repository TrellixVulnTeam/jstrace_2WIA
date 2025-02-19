// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "src/objects.h"

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

#include "src/code-factory.h"
#include "src/elements.h"

namespace v8 {
    namespace internal {

	namespace {

	    inline bool ClampedToInteger(Isolate* isolate, Object* object, int* out) {
		// This is an extended version of ECMA-262 7.1.11 handling signed values
		// Try to convert object to a number and clamp values to [kMinInt, kMaxInt]
		if (object->IsSmi()) {
		    *out = Smi::cast(object)->value();
		    return true;
		} else if (object->IsHeapNumber()) {
		    double value = HeapNumber::cast(object)->value();
		    if (std::isnan(value)) {
			*out = 0;
		    } else if (value > kMaxInt) {
			*out = kMaxInt;
		    } else if (value < kMinInt) {
			*out = kMinInt;
		    } else {
			*out = static_cast<int>(value);
		    }
		    return true;
		} else if (object->IsUndefined(isolate) || object->IsNull(isolate)) {
		    *out = 0;
		    return true;
		} else if (object->IsBoolean()) {
		    *out = object->IsTrue(isolate);
		    return true;
		}
		return false;
	    }

	    inline bool GetSloppyArgumentsLength(Isolate* isolate, Handle<JSObject> object,
						 int* out) {
		Context* context = *isolate->native_context();
		Map* map = object->map();
		if (map != context->sloppy_arguments_map() &&
		    map != context->strict_arguments_map() &&
		    map != context->fast_aliased_arguments_map()) {
		    return false;
		}
		DCHECK(object->HasFastElements() || object->HasFastArgumentsElements());
		Object* len_obj = object->InObjectPropertyAt(JSArgumentsObject::kLengthIndex);
		if (!len_obj->IsSmi()) return false;
		*out = Max(0, Smi::cast(len_obj)->value());
		return *out <= object->elements()->length();
	    }

	    inline bool IsJSArrayFastElementMovingAllowed(Isolate* isolate,
							  JSArray* receiver) {
		return JSObject::PrototypeHasNoElements(isolate, receiver);
	    }

	    inline bool HasSimpleElements(JSObject* current) {
		return current->map()->instance_type() > LAST_CUSTOM_ELEMENTS_RECEIVER &&
		    !current->GetElementsAccessor()->HasAccessors(current);
	    }

	    inline bool HasOnlySimpleReceiverElements(Isolate* isolate,
						      JSObject* receiver) {
		// Check that we have no accessors on the receiver's elements.
		if (!HasSimpleElements(receiver)) return false;
		return JSObject::PrototypeHasNoElements(isolate, receiver);
	    }

	    inline bool HasOnlySimpleElements(Isolate* isolate, JSReceiver* receiver) {
		DisallowHeapAllocation no_gc;
		PrototypeIterator iter(isolate, receiver, kStartAtReceiver);
		for (; !iter.IsAtEnd(); iter.Advance()) {
		    if (iter.GetCurrent()->IsJSProxy()) return false;
		    JSObject* current = iter.GetCurrent<JSObject>();
		    if (!HasSimpleElements(current)) return false;
		}
		return true;
	    }

	    // Returns |false| if not applicable.
	    MUST_USE_RESULT
	    inline bool EnsureJSArrayWithWritableFastElements(Isolate* isolate,
							      Handle<Object> receiver,
							      BuiltinArguments* args,
							      int first_added_arg) {
		if (!receiver->IsJSArray()) return false;
		Handle<JSArray> array = Handle<JSArray>::cast(receiver);
		ElementsKind origin_kind = array->GetElementsKind();
		if (IsDictionaryElementsKind(origin_kind)) return false;
		if (!array->map()->is_extensible()) return false;
		if (args == nullptr) return true;

		// If there may be elements accessors in the prototype chain, the fast path
		// cannot be used if there arguments to add to the array.
		if (!IsJSArrayFastElementMovingAllowed(isolate, *array)) return false;

		// Adding elements to the array prototype would break code that makes sure
		// it has no elements. Handle that elsewhere.
		if (isolate->IsAnyInitialArrayPrototype(array)) return false;

		// Need to ensure that the arguments passed in args can be contained in
		// the array.
		int args_length = args->length();
		if (first_added_arg >= args_length) return true;

		if (IsFastObjectElementsKind(origin_kind)) return true;
		ElementsKind target_kind = origin_kind;
		{
		    DisallowHeapAllocation no_gc;
		    for (int i = first_added_arg; i < args_length; i++) {
			Object* arg = (*args)[i];
			if (arg->IsHeapObject()) {
			    if (arg->IsHeapNumber()) {
				target_kind = FAST_DOUBLE_ELEMENTS;
			    } else {
				target_kind = FAST_ELEMENTS;
				break;
			    }
			}
		    }
		}
		if (target_kind != origin_kind) {
		    // Use a short-lived HandleScope to avoid creating several copies of the
		    // elements handle which would cause issues when left-trimming later-on.
		    HandleScope scope(isolate);
		    JSObject::TransitionElementsKind(array, target_kind);
		}
		return true;
	    }

	    MUST_USE_RESULT static Object* CallJsIntrinsic(Isolate* isolate,
							   Handle<JSFunction> function,
							   BuiltinArguments args) {
		HandleScope handleScope(isolate);
		int argc = args.length() - 1;
		ScopedVector<Handle<Object>> argv(argc);
		for (int i = 0; i < argc; ++i) {
		    argv[i] = args.at<Object>(i + 1);
		}
		RETURN_RESULT_OR_FAILURE(
					 isolate,
					 Execution::Call(isolate, function, args.receiver(), argc, argv.start()));
	    }

	    Object* DoArrayPush(Isolate* isolate, BuiltinArguments args) {
		HandleScope scope(isolate);
		Handle<Object> receiver = args.receiver();

		// try yacheng
		for (int i = 0; i < args.length(); i++) {
		    //std::cout << "DoArrayPush args: " << args[i] << "\n";
		}
		if (args.length() == 2) {
		    std::stringstream from, target;
		    from << args[1];
		    target << args[0];
		    if (LookBackMap::contains(from.str()) && !LookBackMap::contains(target.str())) {
			LookBackMap::assign(target.str(), from.str());
		    }
		}

		if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 1)) {
		    return CallJsIntrinsic(isolate, isolate->array_push(), args);
		}
		// Fast Elements Path
		int to_add = args.length() - 1;
		Handle<JSArray> array = Handle<JSArray>::cast(receiver);
		int len = Smi::cast(array->length())->value();
		if (to_add == 0) return Smi::FromInt(len);

		// Currently fixed arrays cannot grow too big, so we should never hit this.
		DCHECK_LE(to_add, Smi::kMaxValue - Smi::cast(array->length())->value());

		if (JSArray::HasReadOnlyLength(array)) {
		    return CallJsIntrinsic(isolate, isolate->array_push(), args);
		}

		ElementsAccessor* accessor = array->GetElementsAccessor();
		int new_length = accessor->Push(array, &args, to_add);
		return Smi::FromInt(new_length);
	    }
	}  // namespace

	BUILTIN(ArrayPush) { return DoArrayPush(isolate, args); }

	// TODO(verwaest): This is a temporary helper until the FastArrayPush stub can
	// tailcall to the builtin directly.
	RUNTIME_FUNCTION(Runtime_ArrayPush) {
	    DCHECK_EQ(2, args.length());
	    Arguments* incoming = reinterpret_cast<Arguments*>(args[0]);
	    // Rewrap the arguments as builtins arguments.
	    int argc = incoming->length() + BuiltinArguments::kNumExtraArgsWithReceiver;
	    BuiltinArguments caller_args(argc, incoming->arguments() + 1);
	    return DoArrayPush(isolate, caller_args);
	}

	BUILTIN(ArrayPop) {
	    HandleScope scope(isolate);
	    Handle<Object> receiver = args.receiver();
	    if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, nullptr, 0)) {
		return CallJsIntrinsic(isolate, isolate->array_pop(), args);
	    }

	    Handle<JSArray> array = Handle<JSArray>::cast(receiver);

	    uint32_t len = static_cast<uint32_t>(Smi::cast(array->length())->value());
	    if (len == 0) return isolate->heap()->undefined_value();

	    if (JSArray::HasReadOnlyLength(array)) {
		return CallJsIntrinsic(isolate, isolate->array_pop(), args);
	    }

	    Handle<Object> result;
	    if (IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
		// Fast Elements Path
		result = array->GetElementsAccessor()->Pop(array);
	    } else {
		// Use Slow Lookup otherwise
		uint32_t new_length = len - 1;
		ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						   isolate, result, JSReceiver::GetElement(isolate, array, new_length));
		JSArray::SetLength(array, new_length);
	    }
	    return *result;
	}

	BUILTIN(ArrayShift) {
	    HandleScope scope(isolate);
	    Heap* heap = isolate->heap();
	    Handle<Object> receiver = args.receiver();
	    if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, nullptr, 0) ||
		!IsJSArrayFastElementMovingAllowed(isolate, JSArray::cast(*receiver))) {
		return CallJsIntrinsic(isolate, isolate->array_shift(), args);
	    }
	    Handle<JSArray> array = Handle<JSArray>::cast(receiver);

	    int len = Smi::cast(array->length())->value();
	    if (len == 0) return heap->undefined_value();

	    if (JSArray::HasReadOnlyLength(array)) {
		return CallJsIntrinsic(isolate, isolate->array_shift(), args);
	    }

	    Handle<Object> first = array->GetElementsAccessor()->Shift(array);
	    return *first;
	}

	BUILTIN(ArrayUnshift) {
	    HandleScope scope(isolate);
	    Handle<Object> receiver = args.receiver();
	    if (!EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 1)) {
		return CallJsIntrinsic(isolate, isolate->array_unshift(), args);
	    }
	    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
	    int to_add = args.length() - 1;
	    if (to_add == 0) return array->length();

	    // Currently fixed arrays cannot grow too big, so we should never hit this.
	    DCHECK_LE(to_add, Smi::kMaxValue - Smi::cast(array->length())->value());

	    if (JSArray::HasReadOnlyLength(array)) {
		return CallJsIntrinsic(isolate, isolate->array_unshift(), args);
	    }

	    ElementsAccessor* accessor = array->GetElementsAccessor();
	    int new_length = accessor->Unshift(array, &args, to_add);
	    return Smi::FromInt(new_length);
	}

	BUILTIN(ArraySlice) {
	    HandleScope scope(isolate);
	    Handle<Object> receiver = args.receiver();
	    int len = -1;
	    int relative_start = 0;
	    int relative_end = 0;

	    if (receiver->IsJSArray()) {
		DisallowHeapAllocation no_gc;
		JSArray* array = JSArray::cast(*receiver);
		if (V8_UNLIKELY(!array->HasFastElements() ||
				!IsJSArrayFastElementMovingAllowed(isolate, array) ||
				!isolate->IsArraySpeciesLookupChainIntact() ||
				// If this is a subclass of Array, then call out to JS
				!array->HasArrayPrototype(isolate))) {
		    AllowHeapAllocation allow_allocation;
		    return CallJsIntrinsic(isolate, isolate->array_slice(), args);
		}
		len = Smi::cast(array->length())->value();
	    } else if (receiver->IsJSObject() &&
		       GetSloppyArgumentsLength(isolate, Handle<JSObject>::cast(receiver),
						&len)) {
		// Array.prototype.slice.call(arguments, ...) is quite a common idiom
		// (notably more than 50% of invocations in Web apps).
		// Treat it in C++ as well.
		DCHECK(JSObject::cast(*receiver)->HasFastElements() ||
		       JSObject::cast(*receiver)->HasFastArgumentsElements());
	    } else {
		AllowHeapAllocation allow_allocation;
		return CallJsIntrinsic(isolate, isolate->array_slice(), args);
	    }
	    DCHECK_LE(0, len);
	    int argument_count = args.length() - 1;
	    // Note carefully chosen defaults---if argument is missing,
	    // it's undefined which gets converted to 0 for relative_start
	    // and to len for relative_end.
	    relative_start = 0;
	    relative_end = len;
	    if (argument_count > 0) {
		DisallowHeapAllocation no_gc;
		if (!ClampedToInteger(isolate, args[1], &relative_start)) {
		    AllowHeapAllocation allow_allocation;
		    return CallJsIntrinsic(isolate, isolate->array_slice(), args);
		}
		if (argument_count > 1) {
		    Object* end_arg = args[2];
		    // slice handles the end_arg specially
		    if (end_arg->IsUndefined(isolate)) {
			relative_end = len;
		    } else if (!ClampedToInteger(isolate, end_arg, &relative_end)) {
			AllowHeapAllocation allow_allocation;
			return CallJsIntrinsic(isolate, isolate->array_slice(), args);
		    }
		}
	    }

	    // ECMAScript 232, 3rd Edition, Section 15.4.4.10, step 6.
	    uint32_t actual_start = (relative_start < 0) ? Max(len + relative_start, 0)
		: Min(relative_start, len);

	    // ECMAScript 232, 3rd Edition, Section 15.4.4.10, step 8.
	    uint32_t actual_end =
		(relative_end < 0) ? Max(len + relative_end, 0) : Min(relative_end, len);

	    Handle<JSObject> object = Handle<JSObject>::cast(receiver);
	    ElementsAccessor* accessor = object->GetElementsAccessor();
	    return *accessor->Slice(object, actual_start, actual_end);
	}

	BUILTIN(ArraySplice) {
	    HandleScope scope(isolate);
	    Handle<Object> receiver = args.receiver();
	    if (V8_UNLIKELY(
			    !EnsureJSArrayWithWritableFastElements(isolate, receiver, &args, 3) ||
			    // If this is a subclass of Array, then call out to JS.
			    !Handle<JSArray>::cast(receiver)->HasArrayPrototype(isolate) ||
			    // If anything with @@species has been messed with, call out to JS.
			    !isolate->IsArraySpeciesLookupChainIntact())) {
		return CallJsIntrinsic(isolate, isolate->array_splice(), args);
	    }
	    Handle<JSArray> array = Handle<JSArray>::cast(receiver);

	    int argument_count = args.length() - 1;
	    int relative_start = 0;
	    if (argument_count > 0) {
		DisallowHeapAllocation no_gc;
		if (!ClampedToInteger(isolate, args[1], &relative_start)) {
		    AllowHeapAllocation allow_allocation;
		    return CallJsIntrinsic(isolate, isolate->array_splice(), args);
		}
	    }
	    int len = Smi::cast(array->length())->value();
	    // clip relative start to [0, len]
	    int actual_start = (relative_start < 0) ? Max(len + relative_start, 0)
		: Min(relative_start, len);

	    int actual_delete_count;
	    if (argument_count == 1) {
		// SpiderMonkey, TraceMonkey and JSC treat the case where no delete count is
		// given as a request to delete all the elements from the start.
		// And it differs from the case of undefined delete count.
		// This does not follow ECMA-262, but we do the same for compatibility.
		DCHECK(len - actual_start >= 0);
		actual_delete_count = len - actual_start;
	    } else {
		int delete_count = 0;
		DisallowHeapAllocation no_gc;
		if (argument_count > 1) {
		    if (!ClampedToInteger(isolate, args[2], &delete_count)) {
			AllowHeapAllocation allow_allocation;
			return CallJsIntrinsic(isolate, isolate->array_splice(), args);
		    }
		}
		actual_delete_count = Min(Max(delete_count, 0), len - actual_start);
	    }

	    int add_count = (argument_count > 1) ? (argument_count - 2) : 0;
	    int new_length = len - actual_delete_count + add_count;

	    if (new_length != len && JSArray::HasReadOnlyLength(array)) {
		AllowHeapAllocation allow_allocation;
		return CallJsIntrinsic(isolate, isolate->array_splice(), args);
	    }
	    ElementsAccessor* accessor = array->GetElementsAccessor();
	    Handle<JSArray> result_array = accessor->Splice(
							    array, actual_start, actual_delete_count, &args, add_count);
	    return *result_array;
	}

	// Array Concat -------------------------------------------------------------

	namespace {

	    /**
	     * A simple visitor visits every element of Array's.
	     * The backend storage can be a fixed array for fast elements case,
	     * or a dictionary for sparse array. Since Dictionary is a subtype
	     * of FixedArray, the class can be used by both fast and slow cases.
	     * The second parameter of the constructor, fast_elements, specifies
	     * whether the storage is a FixedArray or Dictionary.
	     *
	     * An index limit is used to deal with the situation that a result array
	     * length overflows 32-bit non-negative integer.
	     */
	    class ArrayConcatVisitor {
	    public:
		ArrayConcatVisitor(Isolate* isolate, Handle<Object> storage,
				   bool fast_elements)
		    : isolate_(isolate),
		      storage_(isolate->global_handles()->Create(*storage)),
		      index_offset_(0u),
		      bit_field_(FastElementsField::encode(fast_elements) |
				 ExceedsLimitField::encode(false) |
				 IsFixedArrayField::encode(storage->IsFixedArray())) {
		    DCHECK(!(this->fast_elements() && !is_fixed_array()));
		}

		~ArrayConcatVisitor() { clear_storage(); }

		MUST_USE_RESULT bool visit(uint32_t i, Handle<Object> elm) {
		    uint32_t index = index_offset_ + i;

		    if (i >= JSObject::kMaxElementCount - index_offset_) {
			set_exceeds_array_limit(true);
			// Exception hasn't been thrown at this point. Return true to
			// break out, and caller will throw. !visit would imply that
			// there is already a pending exception.
			return true;
		    }

		    if (!is_fixed_array()) {
			LookupIterator it(isolate_, storage_, index, LookupIterator::OWN);
			MAYBE_RETURN(
				     JSReceiver::CreateDataProperty(&it, elm, Object::THROW_ON_ERROR),
				     false);
			return true;
		    }

		    if (fast_elements()) {
			if (index < static_cast<uint32_t>(storage_fixed_array()->length())) {
			    storage_fixed_array()->set(index, *elm);
			    return true;
			}
			// Our initial estimate of length was foiled, possibly by
			// getters on the arrays increasing the length of later arrays
			// during iteration.
			// This shouldn't happen in anything but pathological cases.
			SetDictionaryMode();
			// Fall-through to dictionary mode.
		    }
		    DCHECK(!fast_elements());
		    Handle<SeededNumberDictionary> dict(
							SeededNumberDictionary::cast(*storage_));
		    // The object holding this backing store has just been allocated, so
		    // it cannot yet be used as a prototype.
		    Handle<SeededNumberDictionary> result =
			SeededNumberDictionary::AtNumberPut(dict, index, elm, false);
		    if (!result.is_identical_to(dict)) {
			// Dictionary needed to grow.
			clear_storage();
			set_storage(*result);
		    }
		    return true;
		}

		void increase_index_offset(uint32_t delta) {
		    if (JSObject::kMaxElementCount - index_offset_ < delta) {
			index_offset_ = JSObject::kMaxElementCount;
		    } else {
			index_offset_ += delta;
		    }
		    // If the initial length estimate was off (see special case in visit()),
		    // but the array blowing the limit didn't contain elements beyond the
		    // provided-for index range, go to dictionary mode now.
		    if (fast_elements() &&
			index_offset_ >
			static_cast<uint32_t>(FixedArrayBase::cast(*storage_)->length())) {
			SetDictionaryMode();
		    }
		}

		bool exceeds_array_limit() const {
		    return ExceedsLimitField::decode(bit_field_);
		}

		Handle<JSArray> ToArray() {
		    DCHECK(is_fixed_array());
		    Handle<JSArray> array = isolate_->factory()->NewJSArray(0);
		    Handle<Object> length =
			isolate_->factory()->NewNumber(static_cast<double>(index_offset_));
		    Handle<Map> map = JSObject::GetElementsTransitionMap(
									 array, fast_elements() ? FAST_HOLEY_ELEMENTS : DICTIONARY_ELEMENTS);
		    array->set_map(*map);
		    array->set_length(*length);
		    array->set_elements(*storage_fixed_array());
		    return array;
		}

		// Storage is either a FixedArray (if is_fixed_array()) or a JSReciever
		// (otherwise)
		Handle<FixedArray> storage_fixed_array() {
		    DCHECK(is_fixed_array());
		    return Handle<FixedArray>::cast(storage_);
		}
		Handle<JSReceiver> storage_jsreceiver() {
		    DCHECK(!is_fixed_array());
		    return Handle<JSReceiver>::cast(storage_);
		}

	    private:
		// Convert storage to dictionary mode.
		void SetDictionaryMode() {
		    DCHECK(fast_elements() && is_fixed_array());
		    Handle<FixedArray> current_storage = storage_fixed_array();
		    Handle<SeededNumberDictionary> slow_storage(
								SeededNumberDictionary::New(isolate_, current_storage->length()));
		    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
		    FOR_WITH_HANDLE_SCOPE(
					  isolate_, uint32_t, i = 0, i, i < current_length, i++, {
					      Handle<Object> element(current_storage->get(i), isolate_);
					      if (!element->IsTheHole(isolate_)) {
						  // The object holding this backing store has just been allocated, so
						  // it cannot yet be used as a prototype.
						  Handle<SeededNumberDictionary> new_storage =
						      SeededNumberDictionary::AtNumberPut(slow_storage, i, element,
											  false);
						  if (!new_storage.is_identical_to(slow_storage)) {
						      slow_storage = loop_scope.CloseAndEscape(new_storage);
						  }
					      }
					  });
		    clear_storage();
		    set_storage(*slow_storage);
		    set_fast_elements(false);
		}

		inline void clear_storage() { GlobalHandles::Destroy(storage_.location()); }

		inline void set_storage(FixedArray* storage) {
		    DCHECK(is_fixed_array());
		    storage_ = isolate_->global_handles()->Create(storage);
		}

		class FastElementsField : public BitField<bool, 0, 1> {};
		class ExceedsLimitField : public BitField<bool, 1, 1> {};
		class IsFixedArrayField : public BitField<bool, 2, 1> {};

		bool fast_elements() const { return FastElementsField::decode(bit_field_); }
		void set_fast_elements(bool fast) {
		    bit_field_ = FastElementsField::update(bit_field_, fast);
		}
		void set_exceeds_array_limit(bool exceeds) {
		    bit_field_ = ExceedsLimitField::update(bit_field_, exceeds);
		}
		bool is_fixed_array() const { return IsFixedArrayField::decode(bit_field_); }

		Isolate* isolate_;
		Handle<Object> storage_;  // Always a global handle.
		// Index after last seen index. Always less than or equal to
		// JSObject::kMaxElementCount.
		uint32_t index_offset_;
		uint32_t bit_field_;
	    };

	    uint32_t EstimateElementCount(Handle<JSArray> array) {
		DisallowHeapAllocation no_gc;
		uint32_t length = static_cast<uint32_t>(array->length()->Number());
		int element_count = 0;
		switch (array->GetElementsKind()) {
		case FAST_SMI_ELEMENTS:
		case FAST_HOLEY_SMI_ELEMENTS:
		case FAST_ELEMENTS:
		case FAST_HOLEY_ELEMENTS: {
		    // Fast elements can't have lengths that are not representable by
		    // a 32-bit signed integer.
		    DCHECK(static_cast<int32_t>(FixedArray::kMaxLength) >= 0);
		    int fast_length = static_cast<int>(length);
		    Isolate* isolate = array->GetIsolate();
		    FixedArray* elements = FixedArray::cast(array->elements());
		    for (int i = 0; i < fast_length; i++) {
			if (!elements->get(i)->IsTheHole(isolate)) element_count++;
		    }
		    break;
		}
		case FAST_DOUBLE_ELEMENTS:
		case FAST_HOLEY_DOUBLE_ELEMENTS: {
		    // Fast elements can't have lengths that are not representable by
		    // a 32-bit signed integer.
		    DCHECK(static_cast<int32_t>(FixedDoubleArray::kMaxLength) >= 0);
		    int fast_length = static_cast<int>(length);
		    if (array->elements()->IsFixedArray()) {
			DCHECK(FixedArray::cast(array->elements())->length() == 0);
			break;
		    }
		    FixedDoubleArray* elements = FixedDoubleArray::cast(array->elements());
		    for (int i = 0; i < fast_length; i++) {
			if (!elements->is_the_hole(i)) element_count++;
		    }
		    break;
		}
		case DICTIONARY_ELEMENTS: {
		    SeededNumberDictionary* dictionary =
			SeededNumberDictionary::cast(array->elements());
		    Isolate* isolate = dictionary->GetIsolate();
		    int capacity = dictionary->Capacity();
		    for (int i = 0; i < capacity; i++) {
			Object* key = dictionary->KeyAt(i);
			if (dictionary->IsKey(isolate, key)) {
			    element_count++;
			}
		    }
		    break;
		}
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:

		    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
			// External arrays are always dense.
			return length;
		case NO_ELEMENTS:
		    return 0;
		case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
		case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
		case FAST_STRING_WRAPPER_ELEMENTS:
		case SLOW_STRING_WRAPPER_ELEMENTS:
		    UNREACHABLE();
		    return 0;
		}
		// As an estimate, we assume that the prototype doesn't contain any
		// inherited elements.
		return element_count;
	    }

	    // Used for sorting indices in a List<uint32_t>.
	    int compareUInt32(const uint32_t* ap, const uint32_t* bp) {
		uint32_t a = *ap;
		uint32_t b = *bp;
		return (a == b) ? 0 : (a < b) ? -1 : 1;
	    }

	    void CollectElementIndices(Handle<JSObject> object, uint32_t range,
				       List<uint32_t>* indices) {
		Isolate* isolate = object->GetIsolate();
		ElementsKind kind = object->GetElementsKind();
		switch (kind) {
		case FAST_SMI_ELEMENTS:
		case FAST_ELEMENTS:
		case FAST_HOLEY_SMI_ELEMENTS:
		case FAST_HOLEY_ELEMENTS: {
		    DisallowHeapAllocation no_gc;
		    FixedArray* elements = FixedArray::cast(object->elements());
		    uint32_t length = static_cast<uint32_t>(elements->length());
		    if (range < length) length = range;
		    for (uint32_t i = 0; i < length; i++) {
			if (!elements->get(i)->IsTheHole(isolate)) {
			    indices->Add(i);
			}
		    }
		    break;
		}
		case FAST_HOLEY_DOUBLE_ELEMENTS:
		case FAST_DOUBLE_ELEMENTS: {
		    if (object->elements()->IsFixedArray()) {
			DCHECK(object->elements()->length() == 0);
			break;
		    }
		    Handle<FixedDoubleArray> elements(
						      FixedDoubleArray::cast(object->elements()));
		    uint32_t length = static_cast<uint32_t>(elements->length());
		    if (range < length) length = range;
		    for (uint32_t i = 0; i < length; i++) {
			if (!elements->is_the_hole(i)) {
			    indices->Add(i);
			}
		    }
		    break;
		}
		case DICTIONARY_ELEMENTS: {
		    DisallowHeapAllocation no_gc;
		    SeededNumberDictionary* dict =
			SeededNumberDictionary::cast(object->elements());
		    uint32_t capacity = dict->Capacity();
		    FOR_WITH_HANDLE_SCOPE(isolate, uint32_t, j = 0, j, j < capacity, j++, {
			    Object* k = dict->KeyAt(j);
			    if (!dict->IsKey(isolate, k)) continue;
			    DCHECK(k->IsNumber());
			    uint32_t index = static_cast<uint32_t>(k->Number());
			    if (index < range) {
				indices->Add(index);
			    }
			});
		    break;
		}
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:

		    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
		    {
			uint32_t length = static_cast<uint32_t>(
								FixedArrayBase::cast(object->elements())->length());
			if (range <= length) {
			    length = range;
			    // We will add all indices, so we might as well clear it first
			    // and avoid duplicates.
			    indices->Clear();
			}
			for (uint32_t i = 0; i < length; i++) {
			    indices->Add(i);
			}
			if (length == range) return;  // All indices accounted for already.
			break;
		    }
		case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
		case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
		    ElementsAccessor* accessor = object->GetElementsAccessor();
		    for (uint32_t i = 0; i < range; i++) {
			if (accessor->HasElement(object, i)) {
			    indices->Add(i);
			}
		    }
		    break;
		}
		case FAST_STRING_WRAPPER_ELEMENTS:
		case SLOW_STRING_WRAPPER_ELEMENTS: {
		    DCHECK(object->IsJSValue());
		    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
		    DCHECK(js_value->value()->IsString());
		    Handle<String> string(String::cast(js_value->value()), isolate);
		    uint32_t length = static_cast<uint32_t>(string->length());
		    uint32_t i = 0;
		    uint32_t limit = Min(length, range);
		    for (; i < limit; i++) {
			indices->Add(i);
		    }
		    ElementsAccessor* accessor = object->GetElementsAccessor();
		    for (; i < range; i++) {
			if (accessor->HasElement(object, i)) {
			    indices->Add(i);
			}
		    }
		    break;
		}
		case NO_ELEMENTS:
		    break;
		}

		PrototypeIterator iter(isolate, object);
		if (!iter.IsAtEnd()) {
		    // The prototype will usually have no inherited element indices,
		    // but we have to check.
		    CollectElementIndices(PrototypeIterator::GetCurrent<JSObject>(iter), range,
					  indices);
		}
	    }

	    bool IterateElementsSlow(Isolate* isolate, Handle<JSReceiver> receiver,
				     uint32_t length, ArrayConcatVisitor* visitor) {
		FOR_WITH_HANDLE_SCOPE(isolate, uint32_t, i = 0, i, i < length, ++i, {
			Maybe<bool> maybe = JSReceiver::HasElement(receiver, i);
			if (!maybe.IsJust()) return false;
			if (maybe.FromJust()) {
			    Handle<Object> element_value;
			    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
							     isolate, element_value, JSReceiver::GetElement(isolate, receiver, i),
							     false);
			    if (!visitor->visit(i, element_value)) return false;
			}
		    });
		visitor->increase_index_offset(length);
		return true;
	    }

	    /**
	     * A helper function that visits "array" elements of a JSReceiver in numerical
	     * order.
	     *
	     * The visitor argument called for each existing element in the array
	     * with the element index and the element's value.
	     * Afterwards it increments the base-index of the visitor by the array
	     * length.
	     * Returns false if any access threw an exception, otherwise true.
	     */
	    bool IterateElements(Isolate* isolate, Handle<JSReceiver> receiver,
				 ArrayConcatVisitor* visitor) {
		uint32_t length = 0;

		if (receiver->IsJSArray()) {
		    Handle<JSArray> array = Handle<JSArray>::cast(receiver);
		    length = static_cast<uint32_t>(array->length()->Number());
		} else {
		    Handle<Object> val;
		    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
						     isolate, val, Object::GetLengthFromArrayLike(isolate, receiver), false);
		    // TODO(caitp): Support larger element indexes (up to 2^53-1).
		    if (!val->ToUint32(&length)) {
			length = 0;
		    }
		    // TODO(cbruni): handle other element kind as well
		    return IterateElementsSlow(isolate, receiver, length, visitor);
		}

		if (!HasOnlySimpleElements(isolate, *receiver)) {
		    return IterateElementsSlow(isolate, receiver, length, visitor);
		}
		Handle<JSObject> array = Handle<JSObject>::cast(receiver);

		switch (array->GetElementsKind()) {
		case FAST_SMI_ELEMENTS:
		case FAST_ELEMENTS:
		case FAST_HOLEY_SMI_ELEMENTS:
		case FAST_HOLEY_ELEMENTS: {
		    // Run through the elements FixedArray and use HasElement and GetElement
		    // to check the prototype for missing elements.
		    Handle<FixedArray> elements(FixedArray::cast(array->elements()));
		    int fast_length = static_cast<int>(length);
		    DCHECK(fast_length <= elements->length());
		    FOR_WITH_HANDLE_SCOPE(isolate, int, j = 0, j, j < fast_length, j++, {
			    Handle<Object> element_value(elements->get(j), isolate);
			    if (!element_value->IsTheHole(isolate)) {
				if (!visitor->visit(j, element_value)) return false;
			    } else {
				Maybe<bool> maybe = JSReceiver::HasElement(array, j);
				if (!maybe.IsJust()) return false;
				if (maybe.FromJust()) {
				    // Call GetElement on array, not its prototype, or getters won't
				    // have the correct receiver.
				    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
								     isolate, element_value,
								     JSReceiver::GetElement(isolate, array, j), false);
				    if (!visitor->visit(j, element_value)) return false;
				}
			    }
			});
		    break;
		}
		case FAST_HOLEY_DOUBLE_ELEMENTS:
		case FAST_DOUBLE_ELEMENTS: {
		    // Empty array is FixedArray but not FixedDoubleArray.
		    if (length == 0) break;
		    // Run through the elements FixedArray and use HasElement and GetElement
		    // to check the prototype for missing elements.
		    if (array->elements()->IsFixedArray()) {
			DCHECK(array->elements()->length() == 0);
			break;
		    }
		    Handle<FixedDoubleArray> elements(
						      FixedDoubleArray::cast(array->elements()));
		    int fast_length = static_cast<int>(length);
		    DCHECK(fast_length <= elements->length());
		    FOR_WITH_HANDLE_SCOPE(isolate, int, j = 0, j, j < fast_length, j++, {
			    if (!elements->is_the_hole(j)) {
				double double_value = elements->get_scalar(j);
				Handle<Object> element_value =
								      isolate->factory()->NewNumber(double_value);
				if (!visitor->visit(j, element_value)) return false;
			    } else {
				Maybe<bool> maybe = JSReceiver::HasElement(array, j);
				if (!maybe.IsJust()) return false;
				if (maybe.FromJust()) {
				    // Call GetElement on array, not its prototype, or getters won't
				    // have the correct receiver.
				    Handle<Object> element_value;
				    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
								     isolate, element_value,
								     JSReceiver::GetElement(isolate, array, j), false);
				    if (!visitor->visit(j, element_value)) return false;
				}
			    }
			});
		    break;
		}

		case DICTIONARY_ELEMENTS: {
		    Handle<SeededNumberDictionary> dict(array->element_dictionary());
		    List<uint32_t> indices(dict->Capacity() / 2);
		    // Collect all indices in the object and the prototypes less
		    // than length. This might introduce duplicates in the indices list.
		    CollectElementIndices(array, length, &indices);
		    indices.Sort(&compareUInt32);
		    int n = indices.length();
		    FOR_WITH_HANDLE_SCOPE(isolate, int, j = 0, j, j < n, (void)0, {
			    uint32_t index = indices[j];
			    Handle<Object> element;
			    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
							     isolate, element, JSReceiver::GetElement(isolate, array, index),
							     false);
			    if (!visitor->visit(index, element)) return false;
			    // Skip to next different index (i.e., omit duplicates).
			    do {
				j++;
			    } while (j < n && indices[j] == index);
			});
		    break;
		}
		case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
		case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
		    FOR_WITH_HANDLE_SCOPE(
					  isolate, uint32_t, index = 0, index, index < length, index++, {
					      Handle<Object> element;
					      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
									       isolate, element, JSReceiver::GetElement(isolate, array, index),
									       false);
					      if (!visitor->visit(index, element)) return false;
					  });
		    break;
		}
		case NO_ELEMENTS:
		    break;
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) case TYPE##_ELEMENTS:
		    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
			return IterateElementsSlow(isolate, receiver, length, visitor);
		case FAST_STRING_WRAPPER_ELEMENTS:
		case SLOW_STRING_WRAPPER_ELEMENTS:
		    // |array| is guaranteed to be an array or typed array.
		    UNREACHABLE();
		    break;
		}
		visitor->increase_index_offset(length);
		return true;
	    }

	    static Maybe<bool> IsConcatSpreadable(Isolate* isolate, Handle<Object> obj) {
		HandleScope handle_scope(isolate);
		if (!obj->IsJSReceiver()) return Just(false);
		if (!isolate->IsIsConcatSpreadableLookupChainIntact(JSReceiver::cast(*obj))) {
		    // Slow path if @@isConcatSpreadable has been used.
		    Handle<Symbol> key(isolate->factory()->is_concat_spreadable_symbol());
		    Handle<Object> value;
		    MaybeHandle<Object> maybeValue =
			i::Runtime::GetObjectProperty(isolate, obj, key);
		    if (!maybeValue.ToHandle(&value)) return Nothing<bool>();
		    if (!value->IsUndefined(isolate)) return Just(value->BooleanValue());
		}
		return Object::IsArray(obj);
	    }

	    Object* Slow_ArrayConcat(BuiltinArguments* args, Handle<Object> species,
				     Isolate* isolate) {
		int argument_count = args->length();

		bool is_array_species = *species == isolate->context()->array_function();

		// Pass 1: estimate the length and number of elements of the result.
		// The actual length can be larger if any of the arguments have getters
		// that mutate other arguments (but will otherwise be precise).
		// The number of elements is precise if there are no inherited elements.

		ElementsKind kind = FAST_SMI_ELEMENTS;

		uint32_t estimate_result_length = 0;
		uint32_t estimate_nof_elements = 0;
		FOR_WITH_HANDLE_SCOPE(isolate, int, i = 0, i, i < argument_count, i++, {
			Handle<Object> obj((*args)[i], isolate);
			uint32_t length_estimate;
			uint32_t element_estimate;
			if (obj->IsJSArray()) {
			    Handle<JSArray> array(Handle<JSArray>::cast(obj));
			    length_estimate = static_cast<uint32_t>(array->length()->Number());
			    if (length_estimate != 0) {
				ElementsKind array_kind =
				    GetPackedElementsKind(array->GetElementsKind());
				kind = GetMoreGeneralElementsKind(kind, array_kind);
			    }
			    element_estimate = EstimateElementCount(array);
			} else {
			    if (obj->IsHeapObject()) {
				kind = GetMoreGeneralElementsKind(
								  kind, obj->IsNumber() ? FAST_DOUBLE_ELEMENTS : FAST_ELEMENTS);
			    }
			    length_estimate = 1;
			    element_estimate = 1;
			}
			// Avoid overflows by capping at kMaxElementCount.
			if (JSObject::kMaxElementCount - estimate_result_length < length_estimate) {
			    estimate_result_length = JSObject::kMaxElementCount;
			} else {
			    estimate_result_length += length_estimate;
			}
			if (JSObject::kMaxElementCount - estimate_nof_elements < element_estimate) {
			    estimate_nof_elements = JSObject::kMaxElementCount;
			} else {
			    estimate_nof_elements += element_estimate;
			}
		    });

		// If estimated number of elements is more than half of length, a
		// fixed array (fast case) is more time and space-efficient than a
		// dictionary.
		bool fast_case =
		    is_array_species && (estimate_nof_elements * 2) >= estimate_result_length;

		if (fast_case && kind == FAST_DOUBLE_ELEMENTS) {
		    Handle<FixedArrayBase> storage =
			isolate->factory()->NewFixedDoubleArray(estimate_result_length);
		    int j = 0;
		    bool failure = false;
		    if (estimate_result_length > 0) {
			Handle<FixedDoubleArray> double_storage =
			    Handle<FixedDoubleArray>::cast(storage);
			for (int i = 0; i < argument_count; i++) {
			    Handle<Object> obj((*args)[i], isolate);
			    if (obj->IsSmi()) {
				double_storage->set(j, Smi::cast(*obj)->value());
				j++;
			    } else if (obj->IsNumber()) {
				double_storage->set(j, obj->Number());
				j++;
			    } else {
				DisallowHeapAllocation no_gc;
				JSArray* array = JSArray::cast(*obj);
				uint32_t length = static_cast<uint32_t>(array->length()->Number());
				switch (array->GetElementsKind()) {
				case FAST_HOLEY_DOUBLE_ELEMENTS:
				case FAST_DOUBLE_ELEMENTS: {
				    // Empty array is FixedArray but not FixedDoubleArray.
				    if (length == 0) break;
				    FixedDoubleArray* elements =
					FixedDoubleArray::cast(array->elements());
				    for (uint32_t i = 0; i < length; i++) {
					if (elements->is_the_hole(i)) {
					    // TODO(jkummerow/verwaest): We could be a bit more clever
					    // here: Check if there are no elements/getters on the
					    // prototype chain, and if so, allow creation of a holey
					    // result array.
					    // Same thing below (holey smi case).
					    failure = true;
					    break;
					}
					double double_value = elements->get_scalar(i);
					double_storage->set(j, double_value);
					j++;
				    }
				    break;
				}
				case FAST_HOLEY_SMI_ELEMENTS:
				case FAST_SMI_ELEMENTS: {
				    Object* the_hole = isolate->heap()->the_hole_value();
				    FixedArray* elements(FixedArray::cast(array->elements()));
				    for (uint32_t i = 0; i < length; i++) {
					Object* element = elements->get(i);
					if (element == the_hole) {
					    failure = true;
					    break;
					}
					int32_t int_value = Smi::cast(element)->value();
					double_storage->set(j, int_value);
					j++;
				    }
				    break;
				}
				case FAST_HOLEY_ELEMENTS:
				case FAST_ELEMENTS:
				case DICTIONARY_ELEMENTS:
				case NO_ELEMENTS:
				    DCHECK_EQ(0u, length);
				    break;
				default:
				    UNREACHABLE();
				}
			    }
			    if (failure) break;
			}
		    }
		    if (!failure) {
			return *isolate->factory()->NewJSArrayWithElements(storage, kind, j);
		    }
		    // In case of failure, fall through.
		}

		Handle<Object> storage;
		if (fast_case) {
		    // The backing storage array must have non-existing elements to preserve
		    // holes across concat operations.
		    storage =
			isolate->factory()->NewFixedArrayWithHoles(estimate_result_length);
		} else if (is_array_species) {
		    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
		    uint32_t at_least_space_for =
			estimate_nof_elements + (estimate_nof_elements >> 2);
		    storage = SeededNumberDictionary::New(isolate, at_least_space_for);
		} else {
		    DCHECK(species->IsConstructor());
		    Handle<Object> length(Smi::FromInt(0), isolate);
		    Handle<Object> storage_object;
		    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
						       isolate, storage_object,
						       Execution::New(isolate, species, species, 1, &length));
		    storage = storage_object;
		}

		ArrayConcatVisitor visitor(isolate, storage, fast_case);

		for (int i = 0; i < argument_count; i++) {
		    Handle<Object> obj((*args)[i], isolate);
		    Maybe<bool> spreadable = IsConcatSpreadable(isolate, obj);
		    MAYBE_RETURN(spreadable, isolate->heap()->exception());
		    if (spreadable.FromJust()) {
			Handle<JSReceiver> object = Handle<JSReceiver>::cast(obj);
			if (!IterateElements(isolate, object, &visitor)) {
			    return isolate->heap()->exception();
			}
		    } else {
			if (!visitor.visit(0, obj)) return isolate->heap()->exception();
			visitor.increase_index_offset(1);
		    }
		}

		if (visitor.exceeds_array_limit()) {
		    THROW_NEW_ERROR_RETURN_FAILURE(
						   isolate, NewRangeError(MessageTemplate::kInvalidArrayLength));
		}

		if (is_array_species) {
		    return *visitor.ToArray();
		} else {
		    return *visitor.storage_jsreceiver();
		}
	    }

	    bool IsSimpleArray(Isolate* isolate, Handle<JSArray> obj) {
		DisallowHeapAllocation no_gc;
		Map* map = obj->map();
		// If there is only the 'length' property we are fine.
		if (map->prototype() ==
		    isolate->native_context()->initial_array_prototype() &&
		    map->NumberOfOwnDescriptors() == 1) {
		    return true;
		}
		// TODO(cbruni): slower lookup for array subclasses and support slow
		// @@IsConcatSpreadable lookup.
		return false;
	    }

	    MaybeHandle<JSArray> Fast_ArrayConcat(Isolate* isolate,
						  BuiltinArguments* args) {
		if (!isolate->IsIsConcatSpreadableLookupChainIntact()) {
		    return MaybeHandle<JSArray>();
		}
		// We shouldn't overflow when adding another len.
		const int kHalfOfMaxInt = 1 << (kBitsPerInt - 2);
		STATIC_ASSERT(FixedArray::kMaxLength < kHalfOfMaxInt);
		STATIC_ASSERT(FixedDoubleArray::kMaxLength < kHalfOfMaxInt);
		USE(kHalfOfMaxInt);

		int n_arguments = args->length();
		int result_len = 0;
		{
		    DisallowHeapAllocation no_gc;
		    // Iterate through all the arguments performing checks
		    // and calculating total length.
		    for (int i = 0; i < n_arguments; i++) {
			Object* arg = (*args)[i];
			if (!arg->IsJSArray()) return MaybeHandle<JSArray>();
			if (!HasOnlySimpleReceiverElements(isolate, JSObject::cast(arg))) {
			    return MaybeHandle<JSArray>();
			}
			// TODO(cbruni): support fast concatenation of DICTIONARY_ELEMENTS.
			if (!JSObject::cast(arg)->HasFastElements()) {
			    return MaybeHandle<JSArray>();
			}
			Handle<JSArray> array(JSArray::cast(arg), isolate);
			if (!IsSimpleArray(isolate, array)) {
			    return MaybeHandle<JSArray>();
			}
			// The Array length is guaranted to be <= kHalfOfMaxInt thus we won't
			// overflow.
			result_len += Smi::cast(array->length())->value();
			DCHECK(result_len >= 0);
			// Throw an Error if we overflow the FixedArray limits
			if (FixedDoubleArray::kMaxLength < result_len ||
			    FixedArray::kMaxLength < result_len) {
			    AllowHeapAllocation gc;
			    THROW_NEW_ERROR(isolate,
					    NewRangeError(MessageTemplate::kInvalidArrayLength),
					    JSArray);
			}
		    }
		}
		return ElementsAccessor::Concat(isolate, args, n_arguments, result_len);
	    }

	}  // namespace

	// ES6 22.1.3.1 Array.prototype.concat
	BUILTIN(ArrayConcat) {
	    HandleScope scope(isolate);

	    Handle<Object> receiver = args.receiver();
	    // TODO(bmeurer): Do we really care about the exact exception message here?
	    if (receiver->IsNull(isolate) || receiver->IsUndefined(isolate)) {
		THROW_NEW_ERROR_RETURN_FAILURE(
					       isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
								     isolate->factory()->NewStringFromAsciiChecked(
														   "Array.prototype.concat")));
	    }
	    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
					       isolate, receiver, Object::ToObject(isolate, args.receiver()));
	    args[0] = *receiver;

	    Handle<JSArray> result_array;

	    // Avoid a real species read to avoid extra lookups to the array constructor
	    if (V8_LIKELY(receiver->IsJSArray() &&
			  Handle<JSArray>::cast(receiver)->HasArrayPrototype(isolate) &&
			  isolate->IsArraySpeciesLookupChainIntact())) {
		if (Fast_ArrayConcat(isolate, &args).ToHandle(&result_array)) {
		    return *result_array;
		}
		if (isolate->has_pending_exception()) return isolate->heap()->exception();
	    }
	    // Reading @@species happens before anything else with a side effect, so
	    // we can do it here to determine whether to take the fast path.
	    Handle<Object> species;
	    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
					       isolate, species, Object::ArraySpeciesConstructor(isolate, receiver));
	    if (*species == *isolate->array_function()) {
		if (Fast_ArrayConcat(isolate, &args).ToHandle(&result_array)) {
		    return *result_array;
		}
		if (isolate->has_pending_exception()) return isolate->heap()->exception();
	    }
	    return Slow_ArrayConcat(&args, species, isolate);
	}

	void Builtins::Generate_ArrayIsArray(CodeStubAssembler* assembler) {
	    typedef compiler::Node Node;
	    typedef CodeStubAssembler::Label Label;

	    Node* object = assembler->Parameter(1);
	    Node* context = assembler->Parameter(4);

	    Label call_runtime(assembler), return_true(assembler),
		return_false(assembler);

	    assembler->GotoIf(assembler->WordIsSmi(object), &return_false);
	    Node* instance_type = assembler->LoadInstanceType(object);

	    assembler->GotoIf(assembler->Word32Equal(
						     instance_type, assembler->Int32Constant(JS_ARRAY_TYPE)),
			      &return_true);

	    // TODO(verwaest): Handle proxies in-place.
	    assembler->Branch(assembler->Word32Equal(
						     instance_type, assembler->Int32Constant(JS_PROXY_TYPE)),
			      &call_runtime, &return_false);

	    assembler->Bind(&return_true);
	    assembler->Return(assembler->BooleanConstant(true));

	    assembler->Bind(&return_false);
	    assembler->Return(assembler->BooleanConstant(false));

	    assembler->Bind(&call_runtime);
	    assembler->Return(
			      assembler->CallRuntime(Runtime::kArrayIsArray, context, object));
	}

	void Builtins::Generate_ArrayIncludes(CodeStubAssembler* assembler) {
	    typedef compiler::Node Node;
	    typedef CodeStubAssembler::Label Label;
	    typedef CodeStubAssembler::Variable Variable;

	    Node* array = assembler->Parameter(0);
	    Node* search_element = assembler->Parameter(1);
	    Node* start_from = assembler->Parameter(2);
	    Node* context = assembler->Parameter(3 + 2);

	    Node* int32_zero = assembler->Int32Constant(0);
	    Node* int32_one = assembler->Int32Constant(1);

	    Node* the_hole = assembler->TheHoleConstant();
	    Node* undefined = assembler->UndefinedConstant();
	    Node* heap_number_map = assembler->HeapNumberMapConstant();

	    Variable len_var(assembler, MachineRepresentation::kWord32),
		index_var(assembler, MachineRepresentation::kWord32),
		start_from_var(assembler, MachineRepresentation::kWord32);

	    Label init_k(assembler), return_true(assembler), return_false(assembler),
		call_runtime(assembler);

	    Label init_len(assembler);

	    index_var.Bind(int32_zero);
	    len_var.Bind(int32_zero);

	    // Take slow path if not a JSArray, if retrieving elements requires
	    // traversing prototype, or if access checks are required.
	    assembler->BranchIfFastJSArray(array, context, &init_len, &call_runtime);

	    assembler->Bind(&init_len);
	    {
		// Handle case where JSArray length is not an Smi in the runtime
		Node* len = assembler->LoadObjectField(array, JSArray::kLengthOffset);
		assembler->GotoUnless(assembler->WordIsSmi(len), &call_runtime);

		len_var.Bind(assembler->SmiToWord(len));
		assembler->Branch(assembler->Word32Equal(len_var.value(), int32_zero),
				  &return_false, &init_k);
	    }

	    assembler->Bind(&init_k);
	    {
		Label done(assembler), init_k_smi(assembler), init_k_heap_num(assembler),
		    init_k_zero(assembler), init_k_n(assembler);
		Callable call_to_integer = CodeFactory::ToInteger(assembler->isolate());
		Node* tagged_n = assembler->CallStub(call_to_integer, context, start_from);

		assembler->Branch(assembler->WordIsSmi(tagged_n), &init_k_smi,
				  &init_k_heap_num);

		assembler->Bind(&init_k_smi);
		{
		    start_from_var.Bind(assembler->SmiToWord32(tagged_n));
		    assembler->Goto(&init_k_n);
		}

		assembler->Bind(&init_k_heap_num);
		{
		    Label do_return_false(assembler);
		    Node* fp_len = assembler->ChangeInt32ToFloat64(len_var.value());
		    Node* fp_n = assembler->LoadHeapNumberValue(tagged_n);
		    assembler->GotoIf(assembler->Float64GreaterThanOrEqual(fp_n, fp_len),
				      &do_return_false);
		    start_from_var.Bind(assembler->TruncateFloat64ToWord32(fp_n));
		    assembler->Goto(&init_k_n);

		    assembler->Bind(&do_return_false);
		    {
			index_var.Bind(int32_zero);
			assembler->Goto(&return_false);
		    }
		}

		assembler->Bind(&init_k_n);
		{
		    Label if_positive(assembler), if_negative(assembler), done(assembler);
		    assembler->Branch(
				      assembler->Int32LessThan(start_from_var.value(), int32_zero),
				      &if_negative, &if_positive);

		    assembler->Bind(&if_positive);
		    {
			index_var.Bind(start_from_var.value());
			assembler->Goto(&done);
		    }

		    assembler->Bind(&if_negative);
		    {
			index_var.Bind(
				       assembler->Int32Add(len_var.value(), start_from_var.value()));
			assembler->Branch(
					  assembler->Int32LessThan(index_var.value(), int32_zero),
					  &init_k_zero, &done);
		    }

		    assembler->Bind(&init_k_zero);
		    {
			index_var.Bind(int32_zero);
			assembler->Goto(&done);
		    }

		    assembler->Bind(&done);
		}
	    }

	    static int32_t kElementsKind[] = {
		FAST_SMI_ELEMENTS,   FAST_HOLEY_SMI_ELEMENTS, FAST_ELEMENTS,
		FAST_HOLEY_ELEMENTS, FAST_DOUBLE_ELEMENTS,    FAST_HOLEY_DOUBLE_ELEMENTS,
	    };

	    Label if_smiorobjects(assembler), if_packed_doubles(assembler),
		if_holey_doubles(assembler);
	    Label* element_kind_handlers[] = {&if_smiorobjects,   &if_smiorobjects,
					      &if_smiorobjects,   &if_smiorobjects,
					      &if_packed_doubles, &if_holey_doubles};

	    Node* map = assembler->LoadMap(array);
	    Node* bit_field2 = assembler->LoadMapBitField2(map);
	    Node* elements_kind =
		assembler->BitFieldDecode<Map::ElementsKindBits>(bit_field2);
	    Node* elements = assembler->LoadElements(array);
	    assembler->Switch(elements_kind, &return_false, kElementsKind,
			      element_kind_handlers, arraysize(kElementsKind));

	    assembler->Bind(&if_smiorobjects);
	    {
		Variable search_num(assembler, MachineRepresentation::kFloat64);
		Label ident_loop(assembler, &index_var),
		    heap_num_loop(assembler, &search_num),
		    string_loop(assembler, &index_var), simd_loop(assembler),
		    undef_loop(assembler, &index_var), not_smi(assembler),
		    not_heap_num(assembler);

		assembler->GotoUnless(assembler->WordIsSmi(search_element), &not_smi);
		search_num.Bind(assembler->SmiToFloat64(search_element));
		assembler->Goto(&heap_num_loop);

		assembler->Bind(&not_smi);
		assembler->GotoIf(assembler->WordEqual(search_element, undefined),
				  &undef_loop);
		Node* map = assembler->LoadMap(search_element);
		assembler->GotoIf(assembler->WordNotEqual(map, heap_number_map),
				  &not_heap_num);
		search_num.Bind(assembler->LoadHeapNumberValue(search_element));
		assembler->Goto(&heap_num_loop);

		assembler->Bind(&not_heap_num);
		Node* search_type = assembler->LoadMapInstanceType(map);
		assembler->GotoIf(
				  assembler->Int32LessThan(
							   search_type, assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
				  &string_loop);
		assembler->GotoIf(
				  assembler->WordEqual(search_type,
						       assembler->Int32Constant(SIMD128_VALUE_TYPE)),
				  &simd_loop);
		assembler->Goto(&ident_loop);

		assembler->Bind(&ident_loop);
		{
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);
		    Node* element_k =
			assembler->LoadFixedArrayElement(elements, index_var.value());
		    assembler->GotoIf(assembler->WordEqual(element_k, search_element),
				      &return_true);

		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&ident_loop);
		}

		assembler->Bind(&undef_loop);
		{
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);
		    Node* element_k =
			assembler->LoadFixedArrayElement(elements, index_var.value());
		    assembler->GotoIf(assembler->WordEqual(element_k, undefined),
				      &return_true);
		    assembler->GotoIf(assembler->WordEqual(element_k, the_hole),
				      &return_true);

		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&undef_loop);
		}

		assembler->Bind(&heap_num_loop);
		{
		    Label nan_loop(assembler, &index_var),
			not_nan_loop(assembler, &index_var);
		    assembler->BranchIfFloat64IsNaN(search_num.value(), &nan_loop,
						    &not_nan_loop);

		    assembler->Bind(&not_nan_loop);
		    {
			Label continue_loop(assembler), not_smi(assembler);
			assembler->GotoUnless(
					      assembler->Int32LessThan(index_var.value(), len_var.value()),
					      &return_false);
			Node* element_k =
			    assembler->LoadFixedArrayElement(elements, index_var.value());
			assembler->GotoUnless(assembler->WordIsSmi(element_k), &not_smi);
			assembler->Branch(
					  assembler->Float64Equal(search_num.value(),
								  assembler->SmiToFloat64(element_k)),
					  &return_true, &continue_loop);

			assembler->Bind(&not_smi);
			assembler->GotoIf(assembler->WordNotEqual(assembler->LoadMap(element_k),
								  heap_number_map),
					  &continue_loop);
			assembler->BranchIfFloat64Equal(
							search_num.value(), assembler->LoadHeapNumberValue(element_k),
							&return_true, &continue_loop);

			assembler->Bind(&continue_loop);
			index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
			assembler->Goto(&not_nan_loop);
		    }

		    assembler->Bind(&nan_loop);
		    {
			Label continue_loop(assembler);
			assembler->GotoUnless(
					      assembler->Int32LessThan(index_var.value(), len_var.value()),
					      &return_false);
			Node* element_k =
			    assembler->LoadFixedArrayElement(elements, index_var.value());
			assembler->GotoIf(assembler->WordIsSmi(element_k), &continue_loop);
			assembler->GotoIf(assembler->WordNotEqual(assembler->LoadMap(element_k),
								  heap_number_map),
					  &continue_loop);
			assembler->BranchIfFloat64IsNaN(
							assembler->LoadHeapNumberValue(element_k), &return_true,
							&continue_loop);

			assembler->Bind(&continue_loop);
			index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
			assembler->Goto(&nan_loop);
		    }
		}

		assembler->Bind(&string_loop);
		{
		    Label continue_loop(assembler);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);
		    Node* element_k =
			assembler->LoadFixedArrayElement(elements, index_var.value());
		    assembler->GotoIf(assembler->WordIsSmi(element_k), &continue_loop);
		    assembler->GotoUnless(assembler->Int32LessThan(
								   assembler->LoadInstanceType(element_k),
								   assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
					  &continue_loop);

		    // TODO(bmeurer): Consider inlining the StringEqual logic here.
		    Callable callable = CodeFactory::StringEqual(assembler->isolate());
		    Node* result =
			assembler->CallStub(callable, context, search_element, element_k);
		    assembler->Branch(
				      assembler->WordEqual(assembler->BooleanConstant(true), result),
				      &return_true, &continue_loop);

		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&string_loop);
		}

		assembler->Bind(&simd_loop);
		{
		    Label continue_loop(assembler, &index_var),
			loop_body(assembler, &index_var);
		    Node* map = assembler->LoadMap(search_element);

		    assembler->Goto(&loop_body);
		    assembler->Bind(&loop_body);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);

		    Node* element_k =
			assembler->LoadFixedArrayElement(elements, index_var.value());
		    assembler->GotoIf(assembler->WordIsSmi(element_k), &continue_loop);

		    Node* map_k = assembler->LoadMap(element_k);
		    assembler->BranchIfSimd128Equal(search_element, map, element_k, map_k,
						    &return_true, &continue_loop);

		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&loop_body);
		}
	    }

	    assembler->Bind(&if_packed_doubles);
	    {
		Label nan_loop(assembler, &index_var), not_nan_loop(assembler, &index_var),
		    hole_loop(assembler, &index_var), search_notnan(assembler);
		Variable search_num(assembler, MachineRepresentation::kFloat64);

		assembler->GotoUnless(assembler->WordIsSmi(search_element), &search_notnan);
		search_num.Bind(assembler->SmiToFloat64(search_element));
		assembler->Goto(&not_nan_loop);

		assembler->Bind(&search_notnan);
		assembler->GotoIf(assembler->WordNotEqual(
							  assembler->LoadMap(search_element), heap_number_map),
				  &return_false);

		search_num.Bind(assembler->LoadHeapNumberValue(search_element));

		assembler->BranchIfFloat64IsNaN(search_num.value(), &nan_loop,
						&not_nan_loop);

		// Search for HeapNumber
		assembler->Bind(&not_nan_loop);
		{
		    Label continue_loop(assembler);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);
		    Node* element_k = assembler->LoadFixedDoubleArrayElement(
									     elements, index_var.value(), MachineType::Float64());
		    assembler->BranchIfFloat64Equal(element_k, search_num.value(),
						    &return_true, &continue_loop);
		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&not_nan_loop);
		}

		// Search for NaN
		assembler->Bind(&nan_loop);
		{
		    Label continue_loop(assembler);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);
		    Node* element_k = assembler->LoadFixedDoubleArrayElement(
									     elements, index_var.value(), MachineType::Float64());
		    assembler->BranchIfFloat64IsNaN(element_k, &return_true, &continue_loop);
		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&nan_loop);
		}
	    }

	    assembler->Bind(&if_holey_doubles);
	    {
		Label nan_loop(assembler, &index_var), not_nan_loop(assembler, &index_var),
		    hole_loop(assembler, &index_var), search_notnan(assembler);
		Variable search_num(assembler, MachineRepresentation::kFloat64);

		assembler->GotoUnless(assembler->WordIsSmi(search_element), &search_notnan);
		search_num.Bind(assembler->SmiToFloat64(search_element));
		assembler->Goto(&not_nan_loop);

		assembler->Bind(&search_notnan);
		assembler->GotoIf(assembler->WordEqual(search_element, undefined),
				  &hole_loop);
		assembler->GotoIf(assembler->WordNotEqual(
							  assembler->LoadMap(search_element), heap_number_map),
				  &return_false);

		search_num.Bind(assembler->LoadHeapNumberValue(search_element));

		assembler->BranchIfFloat64IsNaN(search_num.value(), &nan_loop,
						&not_nan_loop);

		// Search for HeapNumber
		assembler->Bind(&not_nan_loop);
		{
		    Label continue_loop(assembler);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);

		    if (kPointerSize == kDoubleSize) {
			Node* element = assembler->LoadFixedDoubleArrayElement(
									       elements, index_var.value(), MachineType::Uint64());
			Node* the_hole = assembler->Int64Constant(kHoleNanInt64);
			assembler->GotoIf(assembler->Word64Equal(element, the_hole),
					  &continue_loop);
		    } else {
			Node* element_upper = assembler->LoadFixedDoubleArrayElement(
										     elements, index_var.value(), MachineType::Uint32(),
										     kIeeeDoubleExponentWordOffset);
			assembler->GotoIf(
					  assembler->Word32Equal(element_upper,
								 assembler->Int32Constant(kHoleNanUpper32)),
					  &continue_loop);
		    }

		    Node* element_k = assembler->LoadFixedDoubleArrayElement(
									     elements, index_var.value(), MachineType::Float64());
		    assembler->BranchIfFloat64Equal(element_k, search_num.value(),
						    &return_true, &continue_loop);
		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&not_nan_loop);
		}

		// Search for NaN
		assembler->Bind(&nan_loop);
		{
		    Label continue_loop(assembler);
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);

		    if (kPointerSize == kDoubleSize) {
			Node* element = assembler->LoadFixedDoubleArrayElement(
									       elements, index_var.value(), MachineType::Uint64());
			Node* the_hole = assembler->Int64Constant(kHoleNanInt64);
			assembler->GotoIf(assembler->Word64Equal(element, the_hole),
					  &continue_loop);
		    } else {
			Node* element_upper = assembler->LoadFixedDoubleArrayElement(
										     elements, index_var.value(), MachineType::Uint32(),
										     kIeeeDoubleExponentWordOffset);
			assembler->GotoIf(
					  assembler->Word32Equal(element_upper,
								 assembler->Int32Constant(kHoleNanUpper32)),
					  &continue_loop);
		    }

		    Node* element_k = assembler->LoadFixedDoubleArrayElement(
									     elements, index_var.value(), MachineType::Float64());
		    assembler->BranchIfFloat64IsNaN(element_k, &return_true, &continue_loop);
		    assembler->Bind(&continue_loop);
		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&nan_loop);
		}

		// Search for the Hole
		assembler->Bind(&hole_loop);
		{
		    assembler->GotoUnless(
					  assembler->Int32LessThan(index_var.value(), len_var.value()),
					  &return_false);

		    if (kPointerSize == kDoubleSize) {
			Node* element = assembler->LoadFixedDoubleArrayElement(
									       elements, index_var.value(), MachineType::Uint64());
			Node* the_hole = assembler->Int64Constant(kHoleNanInt64);
			assembler->GotoIf(assembler->Word64Equal(element, the_hole),
					  &return_true);
		    } else {
			Node* element_upper = assembler->LoadFixedDoubleArrayElement(
										     elements, index_var.value(), MachineType::Uint32(),
										     kIeeeDoubleExponentWordOffset);
			assembler->GotoIf(
					  assembler->Word32Equal(element_upper,
								 assembler->Int32Constant(kHoleNanUpper32)),
					  &return_true);
		    }

		    index_var.Bind(assembler->Int32Add(index_var.value(), int32_one));
		    assembler->Goto(&hole_loop);
		}
	    }

	    assembler->Bind(&return_true);
	    assembler->Return(assembler->BooleanConstant(true));

	    assembler->Bind(&return_false);
	    assembler->Return(assembler->BooleanConstant(false));

	    assembler->Bind(&call_runtime);
	    assembler->Return(assembler->CallRuntime(Runtime::kArrayIncludes_Slow,
						     context, array, search_element,
						     start_from));
	}

    }  // namespace internal
}  // namespace v8
