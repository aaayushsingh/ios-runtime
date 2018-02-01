//
//  ExtVectorTypeInstance.cpp
//  NativeScript
//
//  Created by Teodor Dermendzhiev on 30/01/2018.
//

#include "ExtVectorTypeInstance.h"
#include "FFISimpleType.h"
#include "IndexedRefInstance.h"
#include "Interop.h"
#include "PointerInstance.h"
#include "RecordConstructor.h"
#include "ReferenceInstance.h"
#include "ReferenceTypeInstance.h"
#include "ffi.h"

namespace NativeScript {
using namespace JSC;
typedef ReferenceTypeInstance Base;

const ClassInfo ExtVectorTypeInstance::s_info = { "ExtVectorTypeInstance", &Base::s_info, 0, CREATE_METHOD_TABLE(ExtVectorTypeInstance) };

JSValue ExtVectorTypeInstance::read(ExecState* execState, const void* buffer, JSCell* self) {
    const void* data = buffer; //*reinterpret_cast<void* const*>(buffer);

    if (!data) {
        return jsNull();
    }

    GlobalObject* globalObject = jsCast<GlobalObject*>(execState->lexicalGlobalObject());
    ExtVectorTypeInstance* referenceType = jsCast<ExtVectorTypeInstance*>(self);

    PointerInstance* pointer = jsCast<PointerInstance*>(globalObject->interop()->pointerInstanceForPointer(execState->vm(), const_cast<void*>(data)));
    return IndexedRefInstance::create(execState->vm(), globalObject, globalObject->interop()->extVectorInstanceStructure(), referenceType->innerType(), pointer);
}

void ExtVectorTypeInstance::write(ExecState* execState, const JSValue& value, void* buffer, JSCell* self) {
    ExtVectorTypeInstance* referenceType = jsCast<ExtVectorTypeInstance*>(self);

    if (value.isUndefinedOrNull()) {
        // *reinterpret_cast<void**>(buffer) = nullptr;
        buffer = nullptr;
        return;
    }

    if (IndexedRefInstance* reference = jsDynamicCast<IndexedRefInstance*>(value)) {
        if (!reference->data()) {
            GlobalObject* globalObject = jsCast<GlobalObject*>(execState->lexicalGlobalObject());
            reference->createBackingStorage(execState->vm(), globalObject, execState, referenceType->innerType());
        }
    }

    bool hasHandle;
    void* handle = tryHandleofValue(value, &hasHandle);
    if (!hasHandle) {
        JSC::VM& vm = execState->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSValue exception = createError(execState, WTF::ASCIILiteral("Value is not a reference."));
        scope.throwException(execState, exception);
        return;
    }

    //*reinterpret_cast<void**>(buffer) = handle;
    buffer = handle;
}

const char* ExtVectorTypeInstance::encode(JSCell* cell) {
    ExtVectorTypeInstance* self = jsCast<ExtVectorTypeInstance*>(cell);

    if (!self->_compilerEncoding.empty()) {
        return self->_compilerEncoding.c_str();
    }

    self->_compilerEncoding = "^";
    const FFITypeMethodTable& table = getFFITypeMethodTable(self->_innerType.get());
    self->_compilerEncoding += table.encode(self->_innerType.get());
    return self->_compilerEncoding.c_str();
}

void ExtVectorTypeInstance::finishCreation(JSC::VM& vm, JSCell* innerType) {
    Base::finishCreation(vm);
    ffi_type* innerFFIType = const_cast<ffi_type*>(getFFITypeMethodTable(innerType).ffiType);

    size_t arraySize = this->_size;

    if (this->_size % 2) {
        arraySize = this->_size + 1;
    }

    ffi_type* type = new ffi_type({ .size = arraySize * innerFFIType->size, .alignment = innerFFIType->alignment, .type = FFI_TYPE_STRUCT });

    type->elements = new ffi_type*[arraySize + 1];

    for (size_t i = 0; i < arraySize; i++) {
        type->elements[i] = innerFFIType;
    }

    type->elements[arraySize] = nullptr;
    this->_extVectorType = type;
    this->_ffiTypeMethodTable.ffiType = type;
    this->_ffiTypeMethodTable.read = &read;
    this->_ffiTypeMethodTable.write = &write;

    this->_innerType.set(vm, this, innerType);
}

void ExtVectorTypeInstance::visitChildren(JSC::JSCell* cell, JSC::SlotVisitor& visitor) {
    Base::visitChildren(cell, visitor);

    ExtVectorTypeInstance* object = jsCast<ExtVectorTypeInstance*>(cell);
    visitor.append(&object->_innerType);
}

} // namespace NativeScript