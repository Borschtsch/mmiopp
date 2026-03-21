#pragma once

#include <cstdint>      // std::uint32_t and std::uintptr_t for register widths and addresses.
#include <type_traits>  // std::void_t, std::enable_if_t, and type predicates used by the API.

namespace mmio {

// Rw is the default access tag. It models normal readable and writable storage.
struct Rw {};

// Ro models state that software may observe but must not write.
struct Ro {};

// Wo models command-style state that software may write but must not read.
struct Wo {};

// W1c models bits that are read normally and cleared by writing one.
struct W1c {};

// W1s models bits that are read normally and set by writing one.
struct W1s {};

// W1t models bits that are read normally and toggled by writing one.
struct W1t {};

// W0c models bits that are read normally and cleared by writing zero.
struct W0c {};

// W0s models bits that are read normally and set by writing zero.
struct W0s {};

// Rc models state that clears as a side effect of being read.
struct Rc {};

namespace detail {

// kValueUsageNone means an encoded value exposes no value-based operator.
inline constexpr unsigned kValueUsageNone = 0u;

// kValueUsageAssign enables operator= and set(value).
inline constexpr unsigned kValueUsageAssign = 1u << 0;

// kValueUsageOrAssign enables operator|= for OR-safe encoded values.
inline constexpr unsigned kValueUsageOrAssign = 1u << 1;

// kValueUsagePredicate enables operator& as a typed predicate.
inline constexpr unsigned kValueUsagePredicate = 1u << 2;

// kMaskUsageNone means a typed mask exposes no mask-based operator.
inline constexpr unsigned kMaskUsageNone = 0u;

// kMaskUsageAndAssign enables operator&= for typed masks.
inline constexpr unsigned kMaskUsageAndAssign = 1u << 0;

// kMaskUsageXorAssign enables operator^= for typed masks.
inline constexpr unsigned kMaskUsageXorAssign = 1u << 1;

// AccessTraits converts one semantic access tag into concrete compile-time capabilities.
// Readable controls get<Field>() and predicates.
// ValueWritable controls assignment of encoded values.
// RawSettable controls set<Field>(raw) for value fields.
// OrAssignable controls whether operator|= is legal for encoded values.
// AndMaskable and XorMaskable control mask-based clear and toggle operations.
// SymmetricValue controls whether one helper, value(...), is enough, or whether
// the field definition must expose separate state(...) and action(...) constants.
template <bool Readable,
          bool ValueWritable,
          bool RawSettable,
          bool OrAssignable,
          bool AndMaskable,
          bool XorMaskable,
          bool SymmetricValue = true>
struct AccessTraits {
  // kReadable gates get<Field>() and operator& predicates.
  static constexpr bool kReadable = Readable;

  // kRawSettable gates set<Field>(raw), which performs a read-modify-write.
  static constexpr bool kRawSettable = RawSettable;

  // kSymmetricValue says one helper, value(...), may represent both state and action.
  static constexpr bool kSymmetricValue = SymmetricValue;

  // kReadValueUsage records predicate capability for readable encoded values.
  static constexpr unsigned kReadValueUsage = Readable ? kValueUsagePredicate : kValueUsageNone;

  // kWriteValueUsage records assignment capability for writable encoded values.
  static constexpr unsigned kWriteValueUsage =
      ValueWritable
          ? static_cast<unsigned>(kValueUsageAssign |
                                  (OrAssignable ? kValueUsageOrAssign : kValueUsageNone))
          : kValueUsageNone;

  // kCombinedValueUsage is only meaningful for symmetric fields that share one helper.
  static constexpr unsigned kCombinedValueUsage =
      SymmetricValue ? static_cast<unsigned>(kReadValueUsage | kWriteValueUsage)
                     : kValueUsageNone;

  // kMaskUsage records which mask-style operators are legal for this access flavor.
  static constexpr unsigned kMaskUsage =
      static_cast<unsigned>((AndMaskable ? kMaskUsageAndAssign : kMaskUsageNone) |
                            (XorMaskable ? kMaskUsageXorAssign : kMaskUsageNone));
};

// AccessMap binds each public semantic access tag to one concrete trait bundle.
template <typename AccessTag>
struct AccessMap;

// Rw fields support the full symmetric API, including raw set, |=, &=, and ^=.
template <>
struct AccessMap<Rw> : AccessTraits<true, true, true, true, true, true, true> {};

// Ro fields may be observed but not modified by software.
template <>
struct AccessMap<Ro> : AccessTraits<true, false, false, false, false, false, true> {};

// Wo fields may be written, but software cannot read them back meaningfully.
template <>
struct AccessMap<Wo> : AccessTraits<false, true, false, false, false, false, true> {};

// W1c fields expose readable state plus distinct write actions.
template <>
struct AccessMap<W1c> : AccessTraits<true, true, false, false, false, false, false> {};

// W1s fields expose readable state plus distinct write actions.
template <>
struct AccessMap<W1s> : AccessTraits<true, true, false, false, false, false, false> {};

// W1t fields expose readable state plus distinct write actions.
template <>
struct AccessMap<W1t> : AccessTraits<true, true, false, false, false, false, false> {};

// W0c fields expose readable state plus distinct write actions.
template <>
struct AccessMap<W0c> : AccessTraits<true, true, false, false, false, false, false> {};

// W0s fields expose readable state plus distinct write actions.
template <>
struct AccessMap<W0s> : AccessTraits<true, true, false, false, false, false, false> {};

// Rc fields may be observed, but software does not get a write-side API.
template <>
struct AccessMap<Rc> : AccessTraits<true, false, false, false, false, false, true> {};

}  // namespace detail

// BitMask is forward-declared so fields and registers can name typed masks before definition.
template <typename Def,
          typename T = typename Def::ValueType,
          unsigned Usage = (detail::kMaskUsageAndAssign | detail::kMaskUsageXorAssign)>
class BitMask;

// RegValue is forward-declared so fields can return bit-style encoded values.
template <typename Def,
          unsigned Usage =
              (detail::kValueUsageAssign | detail::kValueUsageOrAssign | detail::kValueUsagePredicate)>
class RegValue;

// AssignValue is forward-declared so value fields can return masked-write encodings.
template <typename Def, unsigned Usage = (detail::kValueUsageAssign | detail::kValueUsagePredicate)>
class AssignValue;

// DefaultFieldAccessOf falls back to Rw when a register definition does not override it.
template <typename Def, typename = void>
struct DefaultFieldAccessOf {
  // Type is the resolved default field access tag for this register definition.
  using Type = Rw;
};

// This specialization uses a register's explicit DefaultFieldAccess when present.
template <typename Def>
struct DefaultFieldAccessOf<Def, std::void_t<typename Def::DefaultFieldAccess>> {
  // Type is the register-provided default field access tag.
  using Type = typename Def::DefaultFieldAccess;
};

// DefaultFieldAccessT is the convenience alias used throughout the API surface.
template <typename Def>
using DefaultFieldAccessT = typename DefaultFieldAccessOf<Def>::Type;

// Reg binds one register definition to one concrete MMIO address.
// The public API lives on this type so the raw pointer dereference stays private.
template <typename Def, std::uintptr_t Address>
struct Reg;

// Field is the common base for bit-style and numeric fields.
template <typename Def,
          unsigned Offset,
          unsigned Width,
          typename AccessTag = DefaultFieldAccessT<Def>>
struct Field;

// BitField is the specialization used for fields represented by named encoded states.
template <typename Def,
          unsigned Offset,
          unsigned Width,
          typename AccessTag = DefaultFieldAccessT<Def>>
struct BitField;

// ValueField is the specialization used for fields that carry numeric payloads.
template <typename Def,
          unsigned Offset,
          unsigned Width,
          typename RawFieldType = typename Def::ValueType,
          typename AccessTag = DefaultFieldAccessT<Def>>
struct ValueField;

// Register is the CRTP base that injects type aliases and default access into one definition.
template <typename Def, typename T = std::uint32_t, typename DefaultAccessTag = Rw>
struct Register;

// This predicate overload handles bit-style encoded values.
template <typename Def, std::uintptr_t Address, unsigned Usage>
bool operator&(const Reg<Def, Address>& lhs, RegValue<Def, Usage> rhs);

// This predicate overload handles assign-style encoded values.
template <typename Def, std::uintptr_t Address, unsigned Usage>
bool operator&(const Reg<Def, Address>& lhs, AssignValue<Def, Usage> rhs);

// BitMask is a typed mask token. It answers "which bits belong to this field or register?"
// It intentionally hides raw integers from application code so mask/value confusion
// is rejected by the type system instead of being debugged at runtime.
template <typename Def, typename T, unsigned Usage>
class BitMask {
 public:
  // DefinitionType ties the mask back to its owning register definition.
  using DefinitionType = Def;

  // ValueType is the raw register width used to carry the mask bits.
  using ValueType = T;

  // kUsage records which mask operators are legal for this token.
  static constexpr unsigned kUsage = Usage;

  // operator| combines masks from the same register and intersects their legal usage bits.
  template <unsigned OtherUsage>
  constexpr BitMask<Def, T, (Usage & OtherUsage)> operator|(
      BitMask<Def, T, OtherUsage> rhs) const noexcept {
    return BitMask<Def, T, (Usage & OtherUsage)>::fromBits(
        static_cast<ValueType>(this->bits | rhs.bits));
  }

  // operator~ creates the complemented mask used by clear-style expressions such as &= ~FIELD::MASK.
  constexpr BitMask operator~() const noexcept {
    return fromBits(static_cast<ValueType>(~this->bits));
  }

 private:
  // bits stores the raw mask pattern behind the typed wrapper.
  ValueType bits{};

  // The constructor stays private so only trusted framework code can manufacture masks.
  constexpr explicit BitMask(ValueType bitsValue) noexcept : bits(bitsValue) {}

  // fromBits is the private factory used by fields, registers, and mask operators.
  static constexpr BitMask fromBits(ValueType bitsValue) noexcept {
    return BitMask(bitsValue);
  }

  // Other BitMask specializations need access so usage-bit intersections can construct new masks.
  template <typename, typename, unsigned>
  friend class BitMask;

  // Register needs access to build its full-register MASK constant.
  template <typename, typename, typename>
  friend struct Register;

  // Field needs access to build its per-field MASK constant.
  template <typename, unsigned, unsigned, typename>
  friend struct Field;

  // Reg needs access to consume the hidden raw bits in &= and ^= implementations.
  template <typename, std::uintptr_t>
  friend struct Reg;
};

// RegValue is the encoded-value type for bit-style fields and other OR-safe writes.
// It carries both the shifted value bits and the field mask so predicates can compare exactly.
template <typename Def, unsigned Usage>
class RegValue {
 public:
  // DefinitionType ties the encoded value back to its owning register definition.
  using DefinitionType = Def;

  // ValueType is the raw register width used to carry the encoded bits.
  using ValueType = typename Def::ValueType;

  // kUsage records which value operators are legal for this encoding.
  static constexpr unsigned kUsage = Usage;

  // operator| combines OR-safe encoded values from the same register definition.
  template <unsigned OtherUsage>
  constexpr RegValue<Def, (Usage & OtherUsage)> operator|(
      RegValue<Def, OtherUsage> rhs) const noexcept {
    return RegValue<Def, (Usage & OtherUsage)>(
        static_cast<ValueType>(this->value | rhs.value),
        static_cast<ValueType>(this->mask | rhs.mask));
  }

  // The definition type itself can act as a local register value container.
  // This conversion builds one local copy from the encoded value.
  operator Def() const {
    Def local{};
    local.set(*this);
    return local;
  }

 private:
  // value stores the encoded bits that should be written or compared.
  ValueType value{};

  // mask stores the affected field bits so predicates and mixed compositions stay exact.
  ValueType mask{};

  // The constructor stays private so only framework helpers can manufacture encoded values.
  constexpr RegValue(ValueType encodedValue, ValueType encodedMask) noexcept
      : value(encodedValue), mask(encodedMask) {}

  // Other RegValue specializations need access so operator| can intersect usage bits.
  template <typename, unsigned>
  friend class RegValue;

  // AssignValue needs access so mixed bit/value composition can reuse the encoding.
  template <typename, unsigned>
  friend class AssignValue;

  // Mixed composition uses this overload when a RegValue appears on the left.
  template <typename D, unsigned LeftUsage, unsigned RightUsage>
  friend constexpr AssignValue<D, (LeftUsage & RightUsage)> operator|(
      RegValue<D, LeftUsage> lhs,
      AssignValue<D, RightUsage> rhs) noexcept;

  // Mixed composition uses this overload when a RegValue appears on the right.
  template <typename D, unsigned LeftUsage, unsigned RightUsage>
  friend constexpr AssignValue<D, (LeftUsage & RightUsage)> operator|(
      AssignValue<D, LeftUsage> lhs,
      RegValue<D, RightUsage> rhs) noexcept;

  // Field uses the private constructor through encode(...).
  template <typename, unsigned, unsigned, typename>
  friend struct Field;

  // ValueField reads the hidden members when converting to AssignValue.
  template <typename, unsigned, unsigned, typename, typename>
  friend struct ValueField;

  // Register reads the hidden members when storing encoded values into a local register copy.
  template <typename, typename, typename>
  friend struct Register;

  // Reg consumes the hidden members in assignment, |=, and predicate paths.
  template <typename, std::uintptr_t>
  friend struct Reg;

  // operator& needs access to the hidden mask and value bits.
  template <typename D, std::uintptr_t Address, unsigned EncodedUsage>
  friend bool operator&(const Reg<D, Address>& lhs, RegValue<D, EncodedUsage> rhs);
};

// AssignValue is the encoded-value type for fields that must preserve zero bits during writes.
// Numeric value fields land here because writing them usually requires masked replacement.
template <typename Def, unsigned Usage>
class AssignValue {
 public:
  // DefinitionType ties the encoded value back to its owning register definition.
  using DefinitionType = Def;

  // ValueType is the raw register width used to carry the encoded bits.
  using ValueType = typename Def::ValueType;

  // kUsage records which value operators are legal for this encoding.
  static constexpr unsigned kUsage = Usage;

  // operator| combines assign-style encoded values from the same register definition.
  template <unsigned OtherUsage>
  constexpr AssignValue<Def, (Usage & OtherUsage)> operator|(
      AssignValue<Def, OtherUsage> rhs) const noexcept {
    return AssignValue<Def, (Usage & OtherUsage)>(
        static_cast<ValueType>(this->value | rhs.value),
        static_cast<ValueType>(this->mask | rhs.mask));
  }

  // The definition type itself can act as a local register value container.
  // This conversion builds one local copy from the encoded value.
  operator Def() const {
    Def local{};
    local.set(*this);
    return local;
  }

 private:
  // value stores the encoded bits that should be written or compared.
  ValueType value{};

  // mask stores the affected field bits so masked replacement knows what to update.
  ValueType mask{};

  // The constructor stays private so only framework helpers can manufacture assign-style values.
  constexpr AssignValue(ValueType encodedValue, ValueType encodedMask) noexcept
      : value(encodedValue), mask(encodedMask) {}

  // RegValue needs access so mixed bit/value composition can promote into AssignValue.
  template <typename, unsigned>
  friend class RegValue;

  // Other AssignValue specializations need access so operator| can intersect usage bits.
  template <typename, unsigned>
  friend class AssignValue;

  // Mixed composition uses this overload when a RegValue appears on the left.
  template <typename D, unsigned LeftUsage, unsigned RightUsage>
  friend constexpr AssignValue<D, (LeftUsage & RightUsage)> operator|(
      RegValue<D, LeftUsage> lhs,
      AssignValue<D, RightUsage> rhs) noexcept;

  // Mixed composition uses this overload when a RegValue appears on the right.
  template <typename D, unsigned LeftUsage, unsigned RightUsage>
  friend constexpr AssignValue<D, (LeftUsage & RightUsage)> operator|(
      AssignValue<D, LeftUsage> lhs,
      RegValue<D, RightUsage> rhs) noexcept;

  // ValueField uses the private constructor through encodeAssign(...).
  template <typename, unsigned, unsigned, typename, typename>
  friend struct ValueField;

  // Register reads the hidden members when storing encoded values into a local register copy.
  template <typename, typename, typename>
  friend struct Register;

  // Reg consumes the hidden members in assignment, set(value), and predicate paths.
  template <typename, std::uintptr_t>
  friend struct Reg;

  // operator& needs access to the hidden mask and value bits.
  template <typename D, std::uintptr_t Address, unsigned EncodedUsage>
  friend bool operator&(const Reg<D, Address>& lhs, AssignValue<D, EncodedUsage> rhs);
};

// This overload promotes a RegValue|AssignValue composition into AssignValue.
template <typename Def, unsigned LeftUsage, unsigned RightUsage>
constexpr AssignValue<Def, (LeftUsage & RightUsage)> operator|(
    RegValue<Def, LeftUsage> lhs,
    AssignValue<Def, RightUsage> rhs) noexcept {
  // ValueType names the raw storage width without repeating the dependent type.
  using ValueType = typename Def::ValueType;

  // The resulting assign value carries the union of encoded bits and masks.
  return AssignValue<Def, (LeftUsage & RightUsage)>(
      static_cast<ValueType>(lhs.value | rhs.value),
      static_cast<ValueType>(lhs.mask | rhs.mask));
}

// This overload preserves the reversed operand order by reusing the same
// mixed-composition implementation.
template <typename Def, unsigned LeftUsage, unsigned RightUsage>
constexpr AssignValue<Def, (LeftUsage & RightUsage)> operator|(
    AssignValue<Def, LeftUsage> lhs,
    RegValue<Def, RightUsage> rhs) noexcept {
  return rhs | lhs;
}

// IsEncodedValue identifies the framework's two encoded-value wrapper types.
template <typename T>
struct IsEncodedValue : std::false_type {};

// RegValue is an encoded value.
template <typename Def, unsigned Usage>
struct IsEncodedValue<RegValue<Def, Usage>> : std::true_type {};

// AssignValue is an encoded value.
template <typename Def, unsigned Usage>
struct IsEncodedValue<AssignValue<Def, Usage>> : std::true_type {};

// isEncodedValueV is the convenient bool form used in SFINAE.
template <typename T>
inline constexpr bool isEncodedValueV = IsEncodedValue<T>::value;

// IsValueField identifies fields that support set<Field>(raw).
template <typename T, typename = void>
struct IsValueField : std::false_type {};

// Value fields advertise a FieldType alias, so this specialization marks them.
template <typename T>
struct IsValueField<T, std::void_t<typename T::FieldType>> : std::true_type {};

// isValueFieldV is the convenient bool form used in static_asserts.
template <typename T>
inline constexpr bool isValueFieldV = IsValueField<T>::value;

// Field is the common foundation for both BitField and ValueField.
// It computes masks, extraction helpers, and compile-time access traits from one definition.
template <typename Def, unsigned Offset, unsigned Width, typename AccessTag>
struct Field {
  // DefinitionType names the owning register definition.
  using DefinitionType = Def;

  // ValueType is the raw storage width of the owning register.
  using ValueType = typename Def::ValueType;

  // Access resolves the public semantic tag into concrete internal capabilities.
  using Access = detail::AccessMap<AccessTag>;

  // kBits is the total number of bits available in the register storage type.
  static constexpr unsigned kBits = sizeof(ValueType) * 8u;

  // kReadable says whether predicates and get<Field>() are legal.
  static constexpr bool kReadable = Access::kReadable;

  // kRawSettable says whether set<Field>(raw) is legal.
  static constexpr bool kRawSettable = Access::kRawSettable;

  // kReadValueUsage carries predicate capability for readable encoded values.
  static constexpr unsigned kReadValueUsage = Access::kReadValueUsage;

  // kWriteValueUsage carries assignment capability for writable encoded values.
  static constexpr unsigned kWriteValueUsage = Access::kWriteValueUsage;

  // kCombinedValueUsage is only meaningful for symmetric fields that share one helper.
  static constexpr unsigned kCombinedValueUsage = Access::kCombinedValueUsage;

  // kMaskUsage carries the legal mask operators for this field.
  static constexpr unsigned kMaskUsage = Access::kMaskUsage;

  // Fields must occupy at least one bit.
  static_assert(Width > 0, "Field width must be non-zero.");

  // Fields must fit entirely inside the owning register storage type.
  static_assert(Offset + Width <= kBits, "Field overruns register.");

 private:
  // valueMask is the unshifted mask for the field width, for example 0b111 for a 3-bit field.
  static constexpr ValueType valueMask() noexcept {
    // A full-width field uses all bits of the storage type.
    if constexpr (Width == kBits) {
      return static_cast<ValueType>(~ValueType{0});
    } else {
      // Narrower fields build the unshifted width mask in the usual way.
      return static_cast<ValueType>((ValueType{1} << Width) - 1);
    }
  }

  // maskBits shifts the width mask into the field's final register position.
  static constexpr ValueType maskBits() noexcept {
    return static_cast<ValueType>(valueMask() << Offset);
  }

 public:
  // MASK is the typed public mask token used with &=, ^=, and mask composition.
  static constexpr BitMask<Def, ValueType, kMaskUsage> MASK =
      BitMask<Def, ValueType, kMaskUsage>::fromBits(maskBits());

  // extract reads the raw field payload from a register word and right-justifies it.
  static constexpr ValueType extract(ValueType raw) noexcept {
    return static_cast<ValueType>((raw & maskBits()) >> Offset);
  }

  // read is the public name used by Reg::get<Field>() to decode a raw register word.
  static constexpr ValueType read(ValueType raw) noexcept { return extract(raw); }

 protected:
  // encode converts an unshifted field payload into a typed encoded register value.
  template <unsigned Usage>
  static constexpr RegValue<Def, Usage> encode(ValueType raw) noexcept {
    return RegValue<Def, Usage>(static_cast<ValueType>((raw & valueMask()) << Offset), maskBits());
  }
};

// BitField models fields whose payloads are exposed as named encoded states.
// Applications use field-specific names such as ENABLE or RESET, not raw value(...).
template <typename Def, unsigned Offset, unsigned Width, typename AccessTag>
struct BitField : Field<Def, Offset, Width, AccessTag> {
 protected:
  // Base names the shared Field implementation.
  using Base = Field<Def, Offset, Width, AccessTag>;

  // ValueType is the register storage width inherited from Field.
  using ValueType = typename Base::ValueType;

  // value is only for symmetric fields, where the same encoding may be read and written.
  static constexpr RegValue<Def, Base::kCombinedValueUsage> value(ValueType raw) noexcept {
    static_assert(Base::Access::kSymmetricValue,
                  "Asymmetric access fields must use state(...) and action(...) instead of value(...).");
    return Base::template encode<Base::kCombinedValueUsage>(raw);
  }

  // This overload accepts enums or small integral literals without exposing raw writes publicly.
  template <typename U>
  static constexpr RegValue<Def, Base::kCombinedValueUsage> value(U raw) noexcept {
    return value(static_cast<ValueType>(raw));
  }

  // state creates a readable encoded value for asymmetric fields such as W1C or W1S.
  static constexpr RegValue<Def, Base::kReadValueUsage> state(ValueType raw) noexcept {
    static_assert(Base::kReadable, "Field is not readable.");
    return Base::template encode<Base::kReadValueUsage>(raw);
  }

  // This overload accepts enums or small integral literals for state(...).
  template <typename U>
  static constexpr RegValue<Def, Base::kReadValueUsage> state(U raw) noexcept {
    return state(static_cast<ValueType>(raw));
  }

  // action creates a writable encoded value for asymmetric fields such as W1C or W1S.
  static constexpr RegValue<Def, Base::kWriteValueUsage> action(ValueType raw) noexcept {
    static_assert(Base::kWriteValueUsage != detail::kValueUsageNone, "Field is not writable.");
    return Base::template encode<Base::kWriteValueUsage>(raw);
  }

  // This overload accepts enums or small integral literals for action(...).
  template <typename U>
  static constexpr RegValue<Def, Base::kWriteValueUsage> action(U raw) noexcept {
    return action(static_cast<ValueType>(raw));
  }
};

template <typename Def,
          unsigned Offset,
          unsigned Width,
          typename RawFieldType,
          typename AccessTag>
struct ValueField : Field<Def, Offset, Width, AccessTag> {
  // Base names the shared Field implementation.
  using Base = Field<Def, Offset, Width, AccessTag>;

  // ValueType is the register storage width inherited from Field.
  using ValueType = typename Base::ValueType;

  // FieldType is the user-facing raw payload type for this value field.
  using FieldType = RawFieldType;

  // DefinitionType names the owning register definition.
  using DefinitionType = Def;

  // kReadValueUsage carries predicate capability for readable numeric states.
  static constexpr unsigned kReadValueUsage = Base::kReadValueUsage;

  // kWriteValueUsage strips out operator|= because numeric writes are not OR-safe.
  static constexpr unsigned kWriteValueUsage =
      static_cast<unsigned>(Base::kWriteValueUsage & ~detail::kValueUsageOrAssign);

  // kCombinedValueUsage is only meaningful for symmetric value fields.
  static constexpr unsigned kCombinedValueUsage =
      Base::Access::kSymmetricValue
          ? static_cast<unsigned>(kReadValueUsage | kWriteValueUsage)
          : detail::kValueUsageNone;

 private:
  // encodeAssign converts the shared RegValue encoding into an AssignValue wrapper.
  template <unsigned Usage>
  static constexpr AssignValue<Def, Usage> encodeAssign(ValueType raw) noexcept {
    // encoded holds the shared shifted value plus the field mask.
    const auto encoded = Base::template encode<Usage>(raw);

    // The returned AssignValue reuses those hidden bits for masked writes and predicates.
    return AssignValue<Def, Usage>(encoded.value, encoded.mask);
  }

 public:
  // value is only for symmetric numeric fields, where one encoding represents both state and action.
  static constexpr AssignValue<Def, kCombinedValueUsage> value(FieldType raw) noexcept {
    static_assert(Base::Access::kSymmetricValue,
                  "Asymmetric access fields must use state(...) and action(...) instead of value(...).");
    return encodeAssign<kCombinedValueUsage>(static_cast<ValueType>(raw));
  }

  // This overload accepts compatible integral types and converts them into FieldType first.
  template <typename U>
  static constexpr AssignValue<Def, kCombinedValueUsage> value(U raw) noexcept {
    return value(FieldType(raw));
  }

  // state creates a readable encoded value for asymmetric numeric fields.
  static constexpr AssignValue<Def, kReadValueUsage> state(FieldType raw) noexcept {
    static_assert(Base::kReadable, "Field is not readable.");
    return encodeAssign<kReadValueUsage>(static_cast<ValueType>(raw));
  }

  // This overload accepts compatible integral types for state(...).
  template <typename U>
  static constexpr AssignValue<Def, kReadValueUsage> state(U raw) noexcept {
    return state(FieldType(raw));
  }

  // action creates a writable encoded value for asymmetric numeric fields.
  static constexpr AssignValue<Def, kWriteValueUsage> action(FieldType raw) noexcept {
    static_assert(kWriteValueUsage != detail::kValueUsageNone, "Field is not writable.");
    return encodeAssign<kWriteValueUsage>(static_cast<ValueType>(raw));
  }

  // This overload accepts compatible integral types for action(...).
  template <typename U>
  static constexpr AssignValue<Def, kWriteValueUsage> action(U raw) noexcept {
    return action(FieldType(raw));
  }

  // read decodes the raw numeric payload from a register word and returns the field type.
  static constexpr FieldType read(ValueType raw) noexcept {
    return FieldType(Base::extract(raw));
  }
};

// Register is the CRTP base used by concrete register definitions.
// It centralizes the storage width, the default field access, and the scoped aliases.
template <typename Def, typename T, typename DefaultAccessTag>
struct Register {
  // ValueType is the raw storage width of the register.
  using ValueType = T;

  // DefaultFieldAccess is the semantic access tag inherited by fields that do not override it.
  using DefaultFieldAccess = DefaultAccessTag;

  // Access resolves the default field access into concrete internal capabilities.
  using Access = detail::AccessMap<DefaultFieldAccess>;

  // MASK is the typed full-register mask used for whole-register clear/toggle operations.
  static constexpr BitMask<Def, T, Access::kMaskUsage> MASK =
      BitMask<Def, T, Access::kMaskUsage>::fromBits(static_cast<T>(~T{0}));

 private:
  // rawValue stores the local software copy of the register bits.
  ValueType rawValue{};

  // Reg may snapshot from or commit to the local register object.
  template <typename, std::uintptr_t>
  friend struct Reg;

  // readRaw returns the local software copy of the register word.
  ValueType readRaw() const { return this->rawValue; }

  // writeRaw updates the local software copy of the register word.
  void writeRaw(ValueType value) { this->rawValue = value; }

  // writeMasked applies a read-modify-write merge for RegValue-based updates.
  template <unsigned Usage>
  void writeMasked(RegValue<Def, Usage> value) {
    this->writeRaw(static_cast<ValueType>((this->readRaw() & ~value.mask) | value.value));
  }

  // writeMasked applies the same read-modify-write merge for AssignValue-based updates.
  template <unsigned Usage>
  void writeMasked(AssignValue<Def, Usage> value) {
    this->writeRaw(static_cast<ValueType>((this->readRaw() & ~value.mask) | value.value));
  }

 public:
  // Instance binds this register definition to one fixed compile-time MMIO address.
  template <std::uintptr_t Address>
  using Instance = mmio::Reg<Def, Address>;

  // BitField is the nested convenience alias used by concrete bit-field definitions.
  template <unsigned Offset, unsigned Width, typename FieldAccessTag = DefaultFieldAccess>
  using BitField = mmio::BitField<Def, Offset, Width, FieldAccessTag>;

  // ValueField is the nested convenience alias used by concrete numeric-field definitions.
  template <unsigned Offset,
            unsigned Width,
            typename RawFieldType = ValueType,
            typename FieldAccessTag = DefaultFieldAccess>
  using ValueField = mmio::ValueField<Def, Offset, Width, RawFieldType, FieldAccessTag>;

  // operator= writes an OR-safe encoded value into the local register copy.
  template <unsigned Usage>
  void operator=(RegValue<Def, Usage> value) {
    static_assert((Usage & detail::kValueUsageAssign) != 0u,
                  "Encoded value is not writable for this field/register access policy.");
    this->writeRaw(value.value);
  }

  // operator= writes an assign-style encoded value into the local register copy.
  template <unsigned Usage>
  void operator=(AssignValue<Def, Usage> value) {
    static_assert((Usage & detail::kValueUsageAssign) != 0u,
                  "Encoded value is not writable for this field/register access policy.");
    this->writeRaw(value.value);
  }

  // Cross-register RegValue assignment is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator=(RegValue<OtherDef, Usage>) = delete;

  // Cross-register AssignValue assignment is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator=(AssignValue<OtherDef, Usage>) = delete;

  // operator|= applies OR-safe encoded values such as individual enable bits.
  template <unsigned Usage>
  void operator|=(RegValue<Def, Usage> value) {
    static_assert((Usage & detail::kValueUsageOrAssign) != 0u,
                  "Encoded value cannot be used with operator|= for this field/register access policy.");
    this->writeRaw(static_cast<ValueType>(this->readRaw() | value.value));
  }

  // Cross-register RegValue |= is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator|=(RegValue<OtherDef, Usage>) = delete;

  // AssignValue |= is forbidden because masked replacements are not OR-safe.
  template <unsigned Usage>
  void operator|=(AssignValue<Def, Usage>) = delete;

  // Cross-register AssignValue |= is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator|=(AssignValue<OtherDef, Usage>) = delete;

  // Mask |= is forbidden because masks belong to &= or ^=, not to set-style writes.
  template <unsigned Usage>
  void operator|=(BitMask<Def, ValueType, Usage>) = delete;

  // Cross-register mask |= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator|=(BitMask<OtherDef, OtherValueType, Usage>) = delete;

  // operator&= applies a typed mask, typically with a complemented field or register mask.
  template <unsigned Usage>
  void operator&=(BitMask<Def, ValueType, Usage> value) {
    static_assert((Usage & detail::kMaskUsageAndAssign) != 0u,
                  "Mask cannot be used with operator&= for this field/register access policy.");
    this->writeRaw(static_cast<ValueType>(this->readRaw() & value.bits));
  }

  // Cross-register mask &= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator&=(BitMask<OtherDef, OtherValueType, Usage>) = delete;

  // operator^= applies a typed mask to toggle bits when the access policy allows it.
  template <unsigned Usage>
  void operator^=(BitMask<Def, ValueType, Usage> value) {
    static_assert((Usage & detail::kMaskUsageXorAssign) != 0u,
                  "Mask cannot be used with operator^= for this field/register access policy.");
    this->writeRaw(static_cast<ValueType>(this->readRaw() ^ value.bits));
  }

  // Cross-register mask ^= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator^=(BitMask<OtherDef, OtherValueType, Usage>) = delete;

  // operator& compares a readable RegValue against the local register copy.
  // Predicates mask first so zero-valued states still compare exactly.
  template <unsigned Usage>
  bool operator&(RegValue<Def, Usage> value) const {
    static_assert((Usage & detail::kValueUsagePredicate) != 0u,
                  "Encoded value is not readable for this field/register access policy.");
    return (this->readRaw() & value.mask) == value.value;
  }

  // Cross-register RegValue predicates are forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  bool operator&(RegValue<OtherDef, Usage>) const = delete;

  // operator& compares a readable AssignValue against the local register copy.
  // Predicates mask first so zero-valued states still compare exactly.
  template <unsigned Usage>
  bool operator&(AssignValue<Def, Usage> value) const {
    static_assert((Usage & detail::kValueUsagePredicate) != 0u,
                  "Encoded value is not readable for this field/register access policy.");
    return (this->readRaw() & value.mask) == value.value;
  }

  // Cross-register AssignValue predicates are forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  bool operator&(AssignValue<OtherDef, Usage>) const = delete;

  // set(value) is the named form of whole-register RegValue assignment.
  template <unsigned Usage>
  void set(RegValue<Def, Usage> value) {
    *this = value;
  }

  // set(value) is the named form of whole-register AssignValue assignment.
  template <unsigned Usage>
  void set(AssignValue<Def, Usage> value) {
    *this = value;
  }

  // Cross-register RegValue set(...) is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void set(RegValue<OtherDef, Usage>) = delete;

  // Cross-register AssignValue set(...) is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void set(AssignValue<OtherDef, Usage>) = delete;

  // set<encodedValue>() is the zero-argument convenience form for named encoded constants.
  template <const auto& encodedValue,
            typename Encoded = std::remove_cv_t<std::remove_reference_t<decltype(encodedValue)>>,
            std::enable_if_t<isEncodedValueV<Encoded>, int> = 0>
  void set() {
    static_assert(std::is_same_v<typename Encoded::DefinitionType, Def>,
                  "Encoded value must belong to the same register definition.");
    this->set(encodedValue);
  }

  // set<Field>(raw) performs a masked value-field replacement when the field semantics allow it.
  template <typename Field, typename U>
  void set(U raw) {
    static_assert(std::is_same_v<typename Field::DefinitionType, Def>,
                  "Field must belong to the same register definition.");
    static_assert(isValueFieldV<Field>,
                  "set<Field>(raw) is only available for value fields; bit fields must use named encoded values.");
    static_assert(Field::kRawSettable,
                  "set<Field>(raw) is not available for this field access policy.");
    this->writeMasked(Field::value(raw));
  }

  // get<Field>() reads and decodes one field from the local register copy.
  template <typename Field>
  auto get() const {
    static_assert(std::is_same_v<typename Field::DefinitionType, Def>,
                  "Field must belong to the same register definition.");
    static_assert(Field::kReadable, "get<Field>() is not available for this field access policy.");
    return Field::read(this->readRaw());
  }
};

// Reg is the concrete access object bound to one MMIO address.
// All raw memory access remains private so application code cannot bypass typed operations.
template <typename Def, std::uintptr_t Address>
struct Reg {
  // ValueType is the raw storage width of the bound register definition.
  using ValueType = typename Def::ValueType;

  // DefinitionType names the owning register definition.
  using DefinitionType = Def;

 private:
  // LocalBase is the storage-owning base subobject used by the local register definition type.
  using LocalBase = Register<Def, typename Def::ValueType, typename Def::DefaultFieldAccess>;

  // storage returns the volatile lvalue that represents the bound MMIO word.
  static volatile ValueType& storage() { return *reinterpret_cast<volatile ValueType*>(Address); }

  // readRaw performs the actual volatile load from the MMIO address.
  static ValueType readRaw() { return storage(); }

  // writeRaw performs the actual volatile store to the MMIO address.
  static void writeRaw(ValueType value) { storage() = value; }

  // writeMasked applies a read-modify-write merge for RegValue-based updates.
  template <unsigned Usage>
  static void writeMasked(RegValue<Def, Usage> value) {
    writeRaw(static_cast<ValueType>((readRaw() & ~value.mask) | value.value));
  }

  // writeMasked applies the same read-modify-write merge for AssignValue-based updates.
  template <unsigned Usage>
  static void writeMasked(AssignValue<Def, Usage> value) {
    writeRaw(static_cast<ValueType>((readRaw() & ~value.mask) | value.value));
  }

 public:
  // operator Def() snapshots the live MMIO register into a local software copy.
  operator Def() const {
    Def local{};
    static_cast<LocalBase&>(local).writeRaw(readRaw());
    return local;
  }

  // operator= writes an OR-safe encoded value directly to the register.
  template <unsigned Usage>
  void operator=(RegValue<Def, Usage> value) const {
    static_assert((Usage & detail::kValueUsageAssign) != 0u,
                  "Encoded value is not writable for this field/register access policy.");
    writeRaw(value.value);
  }

  // operator= writes an assign-style encoded value directly to the register.
  template <unsigned Usage>
  void operator=(AssignValue<Def, Usage> value) const {
    static_assert((Usage & detail::kValueUsageAssign) != 0u,
                  "Encoded value is not writable for this field/register access policy.");
    writeRaw(value.value);
  }

  // Cross-register RegValue assignment is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator=(RegValue<OtherDef, Usage>) const = delete;

  // Cross-register AssignValue assignment is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator=(AssignValue<OtherDef, Usage>) const = delete;

  // operator= commits a local software copy back into the bound MMIO register.
  void operator=(const Def& value) const {
    writeRaw(static_cast<const LocalBase&>(value).readRaw());
  }

  // operator|= applies OR-safe encoded values such as individual enable bits.
  template <unsigned Usage>
  void operator|=(RegValue<Def, Usage> value) const {
    static_assert((Usage & detail::kValueUsageOrAssign) != 0u,
                  "Encoded value cannot be used with operator|= for this field/register access policy.");
    writeRaw(static_cast<ValueType>(readRaw() | value.value));
  }

  // Cross-register RegValue |= is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator|=(RegValue<OtherDef, Usage>) const = delete;

  // AssignValue |= is forbidden because masked replacements are not OR-safe.
  template <unsigned Usage>
  void operator|=(AssignValue<Def, Usage>) const = delete;

  // Cross-register AssignValue |= is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void operator|=(AssignValue<OtherDef, Usage>) const = delete;

  // Mask |= is forbidden because masks belong to &= or ^=, not to set-style writes.
  template <unsigned Usage>
  void operator|=(BitMask<Def, ValueType, Usage>) const = delete;

  // Cross-register mask |= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator|=(BitMask<OtherDef, OtherValueType, Usage>) const = delete;

  // operator&= applies a typed mask, typically with a complemented field or register mask.
  template <unsigned Usage>
  void operator&=(BitMask<Def, ValueType, Usage> value) const {
    static_assert((Usage & detail::kMaskUsageAndAssign) != 0u,
                  "Mask cannot be used with operator&= for this field/register access policy.");
    writeRaw(static_cast<ValueType>(readRaw() & value.bits));
  }

  // Cross-register mask &= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator&=(BitMask<OtherDef, OtherValueType, Usage>) const = delete;

  // operator^= applies a typed mask to toggle bits when the access policy allows it.
  template <unsigned Usage>
  void operator^=(BitMask<Def, ValueType, Usage> value) const {
    static_assert((Usage & detail::kMaskUsageXorAssign) != 0u,
                  "Mask cannot be used with operator^= for this field/register access policy.");
    writeRaw(static_cast<ValueType>(readRaw() ^ value.bits));
  }

  // Cross-register mask ^= is forbidden at compile time.
  template <typename OtherDef, typename OtherValueType, unsigned Usage>
  void operator^=(BitMask<OtherDef, OtherValueType, Usage>) const = delete;

  // operator& for RegValue needs friendship so it can read the hidden raw register state.
  template <typename D, std::uintptr_t BoundAddress, unsigned Usage>
  friend bool operator&(const Reg<D, BoundAddress>& lhs, RegValue<D, Usage> rhs);

  // operator& for AssignValue needs friendship for the same reason.
  template <typename D, std::uintptr_t BoundAddress, unsigned Usage>
  friend bool operator&(const Reg<D, BoundAddress>& lhs, AssignValue<D, Usage> rhs);

  // set(value) is the named form of whole-register RegValue assignment.
  template <unsigned Usage>
  void set(RegValue<Def, Usage> value) const {
    *this = value;
  }

  // set(value) is the named form of whole-register AssignValue assignment.
  template <unsigned Usage>
  void set(AssignValue<Def, Usage> value) const {
    *this = value;
  }

  // Cross-register RegValue set(...) is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void set(RegValue<OtherDef, Usage>) const = delete;

  // Cross-register AssignValue set(...) is forbidden at compile time.
  template <typename OtherDef, unsigned Usage>
  void set(AssignValue<OtherDef, Usage>) const = delete;

  // set(local) is the named form of committing a local software copy.
  void set(const Def& value) const {
    *this = value;
  }

  // set<encodedValue>() is the zero-argument convenience form for named encoded constants.
  template <const auto& encodedValue,
            typename Encoded = std::remove_cv_t<std::remove_reference_t<decltype(encodedValue)>>,
            std::enable_if_t<isEncodedValueV<Encoded>, int> = 0>
  void set() const {
    static_assert(std::is_same_v<typename Encoded::DefinitionType, Def>,
                  "Encoded value must belong to the same register definition.");
    set(encodedValue);
  }

  // set<Field>(raw) performs a masked value-field replacement when the field semantics allow it.
  template <typename Field, typename U>
  void set(U raw) const {
    static_assert(std::is_same_v<typename Field::DefinitionType, Def>,
                  "Field must belong to the same register definition.");
    static_assert(isValueFieldV<Field>,
                  "set<Field>(raw) is only available for value fields; bit fields must use named encoded values.");
    static_assert(Field::kRawSettable,
                  "set<Field>(raw) is not available for this field access policy.");
    writeMasked(Field::value(raw));
  }

  // get<Field>() reads and decodes one field from the live register value.
  template <typename Field>
  auto get() const {
    static_assert(std::is_same_v<typename Field::DefinitionType, Def>,
                  "Field must belong to the same register definition.");
    static_assert(Field::kReadable, "get<Field>() is not available for this field access policy.");
    return Field::read(readRaw());
  }
};

// operator& compares a readable RegValue against the live register contents.
// Predicates mask first so zero-valued states still compare exactly.
template <typename Def, std::uintptr_t Address, unsigned Usage>
bool operator&(const Reg<Def, Address>&, RegValue<Def, Usage> value) {
  static_assert((Usage & detail::kValueUsagePredicate) != 0u,
                "Encoded value is not readable for this field/register access policy.");
  return (Reg<Def, Address>::readRaw() & value.mask) == value.value;
}

// operator& compares a readable AssignValue against the live register contents.
// Predicates mask first so zero-valued states still compare exactly.
template <typename Def, std::uintptr_t Address, unsigned Usage>
bool operator&(const Reg<Def, Address>&, AssignValue<Def, Usage> value) {
  static_assert((Usage & detail::kValueUsagePredicate) != 0u,
                "Encoded value is not readable for this field/register access policy.");
  return (Reg<Def, Address>::readRaw() & value.mask) == value.value;
}

}  // namespace mmio
