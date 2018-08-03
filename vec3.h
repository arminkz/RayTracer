#pragma once
template<typename T>
class Vec3
{
public:
	//3d units
	T x, y, z;

	//constructors
	Vec3() : x(T(0)), y(T(0)), z(T(0)) {}
	Vec3(T xx) : x(xx), y(xx), z(xx) {}
	Vec3(T xx, T yy, T zz) : x(xx), y(yy), z(zz) {}

	//normalize
	Vec3& normalize()
	{
		T nor2 = length2();
		if (nor2 > 0) {
			T invNor = 1 / sqrt(nor2);
			x *= invNor, y *= invNor, z *= invNor;
		}
		return *this;
	}

	//multiply number in vector
	Vec3<T> operator * (const T &f) const { return Vec3<T>(x * f, y * f, z * f); }

	//cross product
	Vec3<T> operator * (const Vec3<T> &v) const { return Vec3<T>(x * v.x, y * v.y, z * v.z); }

	//dot product
	T dot(const Vec3<T> &v) const { return x * v.x + y * v.y + z * v.z; }

	//sum & substract
	Vec3<T> operator - (const Vec3<T> &v) const { return Vec3<T>(x - v.x, y - v.y, z - v.z); }
	Vec3<T> operator + (const Vec3<T> &v) const { return Vec3<T>(x + v.x, y + v.y, z + v.z); }

	Vec3<T>& operator += (const Vec3<T> &v) { x += v.x, y += v.y, z += v.z; return *this; }
	Vec3<T>& operator *= (const Vec3<T> &v) { x *= v.x, y *= v.y, z *= v.z; return *this; }
	
	//unary negetive
	Vec3<T> operator - () const { return Vec3<T>(-x, -y, -z); }

	//length
	T length2() const { return x * x + y * y + z * z; }
	T length() const { return sqrt(length2()); }

	//for cout printing
	friend std::ostream& operator << (std::ostream &os, const Vec3<T> &v)
	{
		os << "[" << v.x << " " << v.y << " " << v.z << "]";
		return os;
	}

};