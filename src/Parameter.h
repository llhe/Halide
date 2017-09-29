#ifndef HALIDE_PARAMETER_H
#define HALIDE_PARAMETER_H

/** \file
 * Defines the internal representation of parameters to halide piplines
 */

#include "Expr.h"
#include "Buffer.h"

namespace Halide {

class DimensionedParam;
struct ExternFuncArgument;

namespace Internal {

class DimensionedParameter;
struct ParameterContents;

/** A reference-counted handle to a parameter to a halide
 * pipeline. May be a scalar parameter or a buffer */
class Parameter {
    IntrusivePtr<ParameterContents> contents;

    void check_defined() const;
    void check_is_buffer() const;
    void check_is_scalar() const;
    void check_dim_ok(int dim) const;

public:
    /** Construct a new undefined handle */
    EXPORT Parameter();

    /** Construct a new parameter of the given type. If the second
     * argument is true, this is a buffer parameter of the given
     * dimensionality, otherwise, it is a scalar parameter (and the
     * dimensionality should be zero). The parameter will be given a
     * unique auto-generated name. */
    EXPORT Parameter(Type t, bool is_buffer, int dimensions);

    /** Construct a new parameter of the given type with name given by
     * the third argument. If the second argument is true, this is a
     * buffer parameter, otherwise, it is a scalar parameter. The
     * third argument gives the dimensionality of the buffer
     * parameter. It should be zero for scalar parameters. If the
     * fifth argument is true, the the name being passed in was
     * explicitly specified (as opposed to autogenerated). If the
     * sixth argument is true, the Parameter is registered in the global
     * ObjectInstanceRegistry. */
    EXPORT Parameter(Type t, bool is_buffer, int dimensions,
                     const std::string &name, bool is_explicit_name = false,
                     bool register_instance = true, bool is_bound_before_lowering = false);

    /** Copy ctor, operator=, and dtor, needed for ObjectRegistry accounting. */
    EXPORT Parameter(const Parameter&);
    EXPORT Parameter& operator=(const Parameter&);
    EXPORT ~Parameter();

    /** Get the type of this parameter */
    EXPORT Type type() const;

    /** Get the dimensionality of this parameter. Zero for scalars. */
    EXPORT int dimensions() const;

    /** Get the name of this parameter */
    EXPORT const std::string &name() const;

    /** Return true iff the name was explicitly specified */
    EXPORT bool is_explicit_name() const;

    /** Return true iff this Parameter is expected to be replaced with a
     * constant at the start of lowering, and thus should not be used to
     * infer arguments */
    EXPORT bool is_bound_before_lowering() const;

    /** Does this parameter refer to a buffer/image? */
    EXPORT bool is_buffer() const;

    /** If the parameter is a scalar parameter, get its currently
     * bound value. Only relevant when jitting */
    template<typename T>
    NO_INLINE T get_scalar() const {
        // Allow get_scalar<uint64_t>() for all Handle types
        user_assert(type() == type_of<T>() || (type().is_handle() && type_of<T>() == UInt(64)))
            << "Can't get Param<" << type()
            << "> as scalar of type " << type_of<T>() << "\n";
        return *((const T *)(get_scalar_address()));
    }

    /** This returns the current value of get_scalar<type()>()
     * as an Expr. */
    EXPORT Expr get_scalar_expr() const;

    /** If the parameter is a scalar parameter, set its current
     * value. Only relevant when jitting */
    template<typename T>
    NO_INLINE void set_scalar(T val) {
        // Allow set_scalar<uint64_t>() for all Handle types
        user_assert(type() == type_of<T>() || (type().is_handle() && type_of<T>() == UInt(64)))
            << "Can't set Param<" << type()
            << "> to scalar of type " << type_of<T>() << "\n";
        *((T *)(get_scalar_address())) = val;
    }

    /** If the parameter is a buffer parameter, get its currently
     * bound buffer. Only relevant when jitting */
    EXPORT Buffer<> get_buffer() const;

    /** If the parameter is a buffer parameter, set its current
     * value. Only relevant when jitting */
    EXPORT void set_buffer(Buffer<> b);

    /** Get the pointer to the current value of the scalar
     * parameter. For a given parameter, this address will never
     * change. Only relevant when jitting. */

    EXPORT void *get_scalar_address() const;

    /** Tests if this handle is the same as another handle */
    EXPORT bool same_as(const Parameter &other) const;

    /** Tests if this handle is non-nullptr */
    EXPORT bool defined() const;

    /** Get and set constraints for the min, extent, stride, and estimates on
     * the min/extent. */
    //@{
    EXPORT void set_min_constraint(int dim, Expr e);
    EXPORT void set_extent_constraint(int dim, Expr e);
    EXPORT void set_stride_constraint(int dim, Expr e);
    EXPORT void set_min_constraint_estimate(int dim, Expr min);
    EXPORT void set_extent_constraint_estimate(int dim, Expr extent);
    EXPORT void set_host_alignment(int bytes);
    EXPORT Expr min_constraint(int dim) const;
    EXPORT Expr extent_constraint(int dim) const;
    EXPORT Expr stride_constraint(int dim) const;
    EXPORT Expr min_constraint_estimate(int dim) const;
    EXPORT Expr extent_constraint_estimate(int dim) const;
    EXPORT int host_alignment() const;
    //@}

    /** Get and set constraints for scalar parameters. These are used
     * directly by Param, so they must be exported. */
    // @{
    EXPORT void set_min_value(Expr e);
    EXPORT Expr get_min_value() const;
    EXPORT void set_max_value(Expr e);
    EXPORT Expr get_max_value() const;
    EXPORT void set_estimate(Expr e);
    EXPORT Expr get_estimate() const;
    // @}
};

class Dimension {
public:
    /** Get an expression representing the minimum coordinates of this image
     * parameter in the given dimension. */
    EXPORT Expr min() const;

    /** Get an expression representing the extent of this image
     * parameter in the given dimension */
    EXPORT Expr extent() const;

    /** Get an expression representing the maximum coordinates of
     * this image parameter in the given dimension. */
    EXPORT Expr max() const;

    /** Get an expression representing the stride of this image in the
     * given dimension */
    EXPORT Expr stride() const;

    /** Get the estimate of the minimum coordinate of this image parameter
     * in the given dimension. Return an undefined expr if the estimate is
     * never specified. */
    EXPORT Expr min_estimate() const;

    /** Get the estimate of the extent of this image parameter in the given
     * dimension. Return an undefined expr if the estimate is never specified. */
    EXPORT Expr extent_estimate() const;

    /** Set the min in a given dimension to equal the given
     * expression. Setting the mins to zero may simplify some
     * addressing math. */
    EXPORT Dimension set_min(Expr e);

    /** Set the extent in a given dimension to equal the given
     * expression. Images passed in that fail this check will generate
     * a runtime error. Returns a reference to the ImageParam so that
     * these calls may be chained.
     *
     * This may help the compiler generate better
     * code. E.g:
     \code
     im.dim(0).set_extent(100);
     \endcode
     * tells the compiler that dimension zero must be of extent 100,
     * which may result in simplification of boundary checks. The
     * value can be an arbitrary expression:
     \code
     im.dim(0).set_extent(im.dim(1).extent());
     \endcode
     * declares that im is a square image (of unknown size), whereas:
     \code
     im.dim(0).set_extent((im.dim(0).extent()/32)*32);
     \endcode
     * tells the compiler that the extent is a multiple of 32. */
    EXPORT Dimension set_extent(Expr e);

    /** Set the stride in a given dimension to equal the given
     * value. This is particularly helpful to set when
     * vectorizing. Known strides for the vectorized dimension
     * generate better code. */
    EXPORT Dimension set_stride(Expr e);

    /** Set the min and extent in one call. */
    EXPORT Dimension set_bounds(Expr min, Expr extent);

    /** Set the estimate of the min in a given dimension to equal the given
     * expression. This value is only used by the auto-scheduler. */
    EXPORT Dimension set_min_estimate(Expr e);

    /** Set the estimate of the extent in a given dimension to equal the given
     * expression. This value is only used by the auto-scheduler. */
    EXPORT Dimension set_extent_estimate(Expr e);

    /** Set the min and extent estimates in one call. These values are only
     * used by the auto-scheduler. */
    EXPORT Dimension set_bounds_estimate(Expr min, Expr extent);

    /** Get a different dimension of the same buffer */
    // @{
    EXPORT Dimension dim(int i);
    EXPORT const Dimension dim(int i) const;
    // @}

private:
    friend class DimensionedParameter;

    /** Construct a Dimension representing dimension d of some
     * Internal::Parameter p. Only friends may construct
     * these. */
    EXPORT Dimension(const Internal::Parameter &p, int d);

    /** Only friends may copy these, too. This prevents
     * users removing constness by making a non-const copy. */
    Dimension(const Dimension &) = default;

    Parameter param;
    int d;
};

/** A DimensionedParameter is an abstract class that provides a way make static
 * promises about the size and stride of Parameter that are input or output buffers.
 * It exists as a way to reduce duplicate code between ImageParam, Input<Buffer<>>,
 * and Output<Buffer<>>. End-user code should never use it directly. */
class DimensionedParameter {
protected:
    virtual ~DimensionedParameter() {}

public:
    /** Get at the internal parameter object representing this ImageParam. */
    EXPORT virtual Internal::Parameter parameter() const = 0;

    /** Get a handle on one of the dimensions for the purposes of
     * inspecting or constraining its min, extent, or stride. */
    EXPORT Internal::Dimension dim(int i);

    /** Get a handle on one of the dimensions for the purposes of
     * inspecting its min, extent, or stride. */
    EXPORT const Internal::Dimension dim(int i) const;

    /** Get the alignment of the host pointer in bytes. Defaults to
     * the size of type. */
    EXPORT int host_alignment() const;

    /** Set the expected alignment of the host pointer in bytes. */
    EXPORT DimensionedParameter &set_host_alignment(int);

    /** Get the dimensionality of this image parameter */
    EXPORT int dimensions() const;

    /** Get an expression giving the minimum coordinate in dimension 0, which
     * by convention is the coordinate of the left edge of the image */
    EXPORT Expr left() const;

    /** Get an expression giving the maximum coordinate in dimension 0, which
     * by convention is the coordinate of the right edge of the image */
    EXPORT Expr right() const;

    /** Get an expression giving the minimum coordinate in dimension 1, which
     * by convention is the top of the image */
    EXPORT Expr top() const;

    /** Get an expression giving the maximum coordinate in dimension 1, which
     * by convention is the bottom of the image */
    EXPORT Expr bottom() const;

    /** Get an expression giving the extent in dimension 0, which by
     * convention is the width of the image */
    EXPORT Expr width() const;

    /** Get an expression giving the extent in dimension 1, which by
     * convention is the height of the image */
    EXPORT Expr height() const;

    /** Get an expression giving the extent in dimension 2, which by
     * convention is the channel-count of the image */
    EXPORT Expr channels() const;

    /** Using a DimensionedParameter as the argument to an external stage treats it
     * as an Expr */
    EXPORT operator ExternFuncArgument() const;

    /** Using a DimensionedParameter as the argument to an RDom treats it
     * as a DimensionedParam */
    EXPORT operator DimensionedParam() const;
};

/** Validate arguments to a call to a func, image or imageparam. */
void check_call_arg_types(const std::string &name, std::vector<Expr> *args, int dims);

}
}

#endif
