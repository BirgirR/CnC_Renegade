/*
**	Tests for wwmath -- the engine's vector, matrix and quaternion maths.
**
**	wwmath is the right place to start testing this engine: it is pure, has no
**	I/O, no device and no global state, and everything else in the tree stands
**	on it. A wrong sign in a matrix multiply here surfaces as inexplicable
**	rendering or physics behaviour a long way away.
*/

#include "wwtest.h"

#include "vector2.h"
#include "vector3.h"
#include "vector4.h"
#include "matrix3d.h"
#include "matrix4.h"
#include "quat.h"
#include "plane.h"
#include "aabox.h"
#include "sphere.h"
#include "lineseg.h"
#include "wwmath.h"

static const float EPS = 1.0e-5f;

//-----------------------------------------------------------------------------
//	Vector3
//-----------------------------------------------------------------------------

WWTEST(Vector3, Length)
{
	Vector3 v(3.0f, 4.0f, 0.0f);
	CHECK_NEAR(v.Length(), 5.0f, EPS);
	CHECK_NEAR(v.Length2(), 25.0f, EPS);

	Vector3 zero(0.0f, 0.0f, 0.0f);
	CHECK_NEAR(zero.Length(), 0.0f, EPS);
}

WWTEST(Vector3, Normalize)
{
	Vector3 v(0.0f, 0.0f, 7.5f);
	v.Normalize();
	CHECK_NEAR(v.Length(), 1.0f, EPS);
	CHECK_NEAR(v.Z, 1.0f, EPS);
}

WWTEST(Vector3, DotAndCross)
{
	Vector3 x(1.0f, 0.0f, 0.0f);
	Vector3 y(0.0f, 1.0f, 0.0f);

	CHECK_NEAR(Vector3::Dot_Product(x, y), 0.0f, EPS);
	CHECK_NEAR(Vector3::Dot_Product(x, x), 1.0f, EPS);

	// Right-handed: x cross y == z
	Vector3 c;
	Vector3::Cross_Product(x, y, &c);
	CHECK_NEAR(c.X, 0.0f, EPS);
	CHECK_NEAR(c.Y, 0.0f, EPS);
	CHECK_NEAR(c.Z, 1.0f, EPS);
}

WWTEST(Vector3, Arithmetic)
{
	Vector3 a(1.0f, 2.0f, 3.0f);
	Vector3 b(4.0f, 5.0f, 6.0f);

	Vector3 sum = a + b;
	CHECK_NEAR(sum.X, 5.0f, EPS);
	CHECK_NEAR(sum.Z, 9.0f, EPS);

	Vector3 diff = b - a;
	CHECK_NEAR(diff.X, 3.0f, EPS);

	Vector3 scaled = a * 2.0f;
	CHECK_NEAR(scaled.Y, 4.0f, EPS);

	CHECK(a == Vector3(1.0f, 2.0f, 3.0f));
	CHECK(a != b);
}

WWTEST(Vector3, Rotate)
{
	// Rotating +X by 90 degrees about Z should land on +Y.
	Vector3 v(1.0f, 0.0f, 0.0f);
	v.Rotate_Z(WWMATH_PI * 0.5f);
	CHECK_NEAR(v.X, 0.0f, 1.0e-4f);
	CHECK_NEAR(v.Y, 1.0f, 1.0e-4f);
}

//-----------------------------------------------------------------------------
//	Vector2
//-----------------------------------------------------------------------------

WWTEST(Vector2, Basics)
{
	Vector2 v(3.0f, 4.0f);
	CHECK_NEAR(v.Length(), 5.0f, EPS);

	v.Normalize();
	CHECK_NEAR(v.Length(), 1.0f, EPS);

	Vector2 a(1.0f, 0.0f);
	Vector2 b(0.0f, 1.0f);
	CHECK_NEAR(Vector2::Dot_Product(a, b), 0.0f, EPS);
}

//-----------------------------------------------------------------------------
//	Matrix3D
//-----------------------------------------------------------------------------

WWTEST(Matrix3D, IdentityIsNeutral)
{
	Matrix3D m(1);		// 1 == make identity
	Vector3 v(1.0f, 2.0f, 3.0f);

	Vector3 out = m * v;
	CHECK_NEAR(out.X, 1.0f, EPS);
	CHECK_NEAR(out.Y, 2.0f, EPS);
	CHECK_NEAR(out.Z, 3.0f, EPS);
}

WWTEST(Matrix3D, Translation)
{
	Matrix3D m(1);
	m.Set_Translation(Vector3(10.0f, 20.0f, 30.0f));

	Vector3 out = m * Vector3(1.0f, 1.0f, 1.0f);
	CHECK_NEAR(out.X, 11.0f, EPS);
	CHECK_NEAR(out.Y, 21.0f, EPS);
	CHECK_NEAR(out.Z, 31.0f, EPS);

	Vector3 t = m.Get_Translation();
	CHECK_NEAR(t.Y, 20.0f, EPS);
}

WWTEST(Matrix3D, InverseRoundTrip)
{
	Matrix3D m(1);
	m.Rotate_Z(0.7f);
	m.Set_Translation(Vector3(5.0f, -2.0f, 1.0f));

	Matrix3D inv;
	m.Get_Orthogonal_Inverse(inv);

	Vector3 v(3.0f, 4.0f, 5.0f);
	Vector3 round_trip = inv * (m * v);

	CHECK_NEAR(round_trip.X, v.X, 1.0e-4f);
	CHECK_NEAR(round_trip.Y, v.Y, 1.0e-4f);
	CHECK_NEAR(round_trip.Z, v.Z, 1.0e-4f);
}

//-----------------------------------------------------------------------------
//	Matrix4
//-----------------------------------------------------------------------------

WWTEST(Matrix4, IdentityMultiply)
{
	Matrix4 a(true);		// identity
	Matrix4 b(true);
	Matrix4 c = a * b;

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			CHECK_NEAR(c[i][j], (i == j) ? 1.0f : 0.0f, EPS);
		}
	}
}

WWTEST(Matrix4, TransposeIsInvolution)
{
	Matrix4 m(true);
	m[0][1] = 2.0f;
	m[2][3] = -4.0f;

	Matrix4 once = m.Transpose();
	Matrix4 twice = once.Transpose();

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			CHECK_NEAR(twice[i][j], m[i][j], EPS);
		}
	}

	// And it actually swapped something.
	CHECK_NEAR(once[1][0], 2.0f, EPS);
}

//-----------------------------------------------------------------------------
//	Quaternion
//-----------------------------------------------------------------------------

WWTEST(Quaternion, IdentityLength)
{
	Quaternion q(true);		// identity
	CHECK_NEAR(q.Length(), 1.0f, EPS);
	CHECK_NEAR(q.W, 1.0f, EPS);
}

WWTEST(Quaternion, NormalizeAndInverse)
{
	Quaternion q(0.3f, 0.5f, 0.1f, 0.8f);
	q.Normalize();
	CHECK_NEAR(q.Length(), 1.0f, EPS);

	Quaternion inv = Inverse(q);
	Quaternion product = q * inv;

	// q * q^-1 should be identity.
	CHECK_NEAR(product.W, 1.0f, 1.0e-4f);
	CHECK_NEAR(product.X, 0.0f, 1.0e-4f);
	CHECK_NEAR(product.Y, 0.0f, 1.0e-4f);
	CHECK_NEAR(product.Z, 0.0f, 1.0e-4f);
}

WWTEST(Quaternion, MatrixRoundTrip)
{
	// A rotation expressed as a quaternion, converted to a matrix and applied,
	// must match the same rotation applied directly.
	Quaternion q = Axis_To_Quat(Vector3(0.0f, 0.0f, 1.0f), WWMATH_PI * 0.5f);
	Matrix3D m = Build_Matrix3D(q);

	Vector3 v(1.0f, 0.0f, 0.0f);
	Vector3 by_matrix = m * v;

	CHECK_NEAR(by_matrix.X, 0.0f, 1.0e-4f);
	CHECK_NEAR(by_matrix.Y, 1.0f, 1.0e-4f);
	CHECK_NEAR(by_matrix.Z, 0.0f, 1.0e-4f);
}

//-----------------------------------------------------------------------------
//	Plane / geometry
//-----------------------------------------------------------------------------

WWTEST(PlaneClass, DistanceSign)
{
	// Plane through the origin facing +Z.
	PlaneClass p(Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f));

	CHECK(p.In_Front(Vector3(0.0f, 0.0f, 5.0f)));
	CHECK(!p.In_Front(Vector3(0.0f, 0.0f, -5.0f)));
}

WWTEST(AABoxClass, ContainsAndExtents)
{
	AABoxClass box(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));

	CHECK_NEAR(box.Center.X, 0.0f, EPS);
	CHECK_NEAR(box.Extent.X, 1.0f, EPS);

	CHECK(box.Contains(Vector3(0.5f, 0.5f, 0.5f)));
	CHECK(!box.Contains(Vector3(2.0f, 0.0f, 0.0f)));
}

WWTEST(SphereClass, UnionGrows)
{
	SphereClass a(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
	SphereClass b(Vector3(10.0f, 0.0f, 0.0f), 1.0f);

	SphereClass u = a;
	u.Add_Sphere(b);

	// The union must contain both originals.
	CHECK(u.Radius >= 6.0f);
	CHECK_NEAR(u.Center.X, 5.0f, 1.0e-3f);
}

//-----------------------------------------------------------------------------
//	WWMath helpers
//-----------------------------------------------------------------------------

WWTEST(WWMath, SqrtAndInvSqrt)
{
	CHECK_NEAR(WWMath::Sqrt(16.0f), 4.0f, 1.0e-4f);
	CHECK_NEAR(WWMath::Sqrt(0.0f), 0.0f, EPS);
	CHECK_NEAR(WWMath::Inv_Sqrt(4.0f), 0.5f, 1.0e-3f);
}

WWTEST(WWMath, ClampAndLerp)
{
	CHECK_NEAR(WWMath::Clamp(5.0f, 0.0f, 1.0f), 1.0f, EPS);
	CHECK_NEAR(WWMath::Clamp(-5.0f, 0.0f, 1.0f), 0.0f, EPS);
	CHECK_NEAR(WWMath::Clamp(0.5f, 0.0f, 1.0f), 0.5f, EPS);

	CHECK_NEAR(WWMath::Lerp(0.0f, 10.0f, 0.25f), 2.5f, EPS);
}
