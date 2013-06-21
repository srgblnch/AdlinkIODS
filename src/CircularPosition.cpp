
#include "CircularPosition.h"
#include "exception.h"
#include <cassert>
#include <string>
#include <sstream>

#if 0
#define ensure(cond) assert(cond)
#else
#define ensure(cond) if(!(cond)) __ensure(__FILE__, __LINE__)
void __ensure(const std::string & fname, unsigned int line, const std::string & msg="")
{
	std::stringstream str;
	str << "ERRORS at " << fname << ":" << line << std::endl;
	str << msg << std::endl;
	throw Exceptions::Exception(str.str());
}
#endif


CircularPosition::CircularPosition() {
	this->m_hasBegun = false;
	this->m_hasLooped = false;
	this->set_limits(0, 5);
	this->starts_with(0);
	this->stop();
}

void CircularPosition::set_limits(CircularPosition::Position min, CircularPosition::Position max)
{
	omni_mutex_lock lock(this->m_mutex);
	ensure(min < max);
	ensure(! this->m_hasBegun);
	
	this->m_minPos = min;
	this->m_maxPos = max;
}

void CircularPosition::starts_with(Position p)
{
	omni_mutex_lock lock(this->m_mutex);
	ensure(p >= this->m_minPos);
	ensure(p <= this->m_maxPos);
	ensure(!this->m_hasBegun);
	
	this->m_first = p;
	this->m_current = p;
	this->m_hasBegun = false;
	this->m_hasLooped = false;
}

void CircularPosition::stop()
{
	omni_mutex_lock lock(this->m_mutex);
	
	this->m_hasBegun = false;
	this->m_hasLooped = false;
	this->m_first = this->m_minPos;
	// first != current, forces a new call to starts_with()
	this->m_current = this->m_minPos + 1;
}

CircularPosition::Position CircularPosition::go_next()
{
	omni_mutex_lock lock(this->m_mutex);
	
	if(!this->m_hasBegun) {
		if(this->m_current != this->m_first)
			throw Exceptions::Exception("Missing call to starts_with()");
		this->m_hasBegun = true;
		return this->m_current;
	}

	this->m_current = this->get_next(this->m_current);
	if(this->m_current == this->m_first)
		this->m_hasLooped = true;
	return this->m_current;
}

CircularPosition::Position CircularPosition::get_next(Position c) const
{
	if (c == this->m_maxPos)
		return this->m_minPos;
	return c + 1;
}

CircularPosition::Position CircularPosition::get_prev(Position c) const
{
	if (c == this->m_minPos)
		return this->m_maxPos;
	return c - 1;
}

/// @return Current bucket being written
CircularPosition::Position CircularPosition::current() const
{
	Position buck;
	ensure(this->current(buck));
	return buck;
}

/// @param cur Current bucket being written
/// @return false if no bucket being written
bool CircularPosition::current(Position & buck) const
{
	omni_mutex_lock lock(this->m_mutex);
	
	buck = this->m_current;
	return this->m_hasBegun;
}

int CircularPosition::distance(Position big, Position low) const
{
	assert(big <= this->m_maxPos);
	assert(big >= this->m_minPos);
	assert(low <= this->m_maxPos);
	assert(low >= this->m_minPos);

	if(big >= low)
		return big - low;
	return big + this->m_maxPos - low - 1;
}

/// Adds a distance to a valid position, giving another valid position
/// as a result.
/// @param[in] p Original position.
/// @param[in] dist Distance where you want to move p.
/// @return A new position.
CircularPosition::Position CircularPosition::move(Position p, int dist) const
{
	// Preconditions:
	// p must be a valid position
	assert( p >= 0 );
	assert( p <= this->m_maxPos && p >= this->m_minPos );
	assert ( this->m_maxPos > this->m_minPos );

	Position res = p + dist;
	if ( res > this->m_maxPos ) {
		res = res - (this->m_maxPos - this->m_minPos);
	}

	if ( res < this->m_minPos ) {
		res = res + (this->m_maxPos - this->m_minPos);
	}

	// Postconditions:
	// aux must be a valid position at the right distance from a.
	assert( res <= this->m_maxPos && res >= this->m_minPos );
	assert( this->distance(p, res) == dist );

	return res;
}

/// @param buck Last bucket written(aka the one just before current())
/// @return false if no bucket written.
bool CircularPosition::last(Position & buck) const
{
	omni_mutex_lock lock(this->m_mutex);
	
	if(!this->m_hasBegun)
		return false;
	if( (!this->m_hasLooped) && (this->m_current == this->m_first) )
		return false;
	buck = this->m_current - 1;
	if (buck < this->m_minPos)
		buck = this->m_maxPos;
	return true;
}

/// @param[in] max How many buckets do we want to get?
/// @param[out] first first (older) bucket
/// @param[out] last last (newest) bucket
/// Note that both first and last are valid buckets, so
/// if max==1, then first=last
bool CircularPosition::last(
						Position max, Position & first, Position & last) const
{
	return this->last(max, 0, first, last);
}

///
/// Get a range of buckets. CircularPosition stores an internal value
/// m_current which is the next of the last bucket written. This function
/// returns lasts 'max' buckets. We can also specify an offset of
/// buckets so we get these zones:
/// 
/// ----------------------
/// | Z1  | Z2 | Z3 | Z4 |
/// ----------------^-----
///                 |
///               m_current
/// 
/// which mean:
///  - Z3 width is 'backOffset' and it is ignored.
///  - Z2 width is at most 'max', and it is the range returned from
///    'first' to 'last'
///  - Z1 and Z4 are unwritten/too long ago written buckets we are not
///    interested in. As this is circular Z1 and Z4 are not distinguishable.
/// 
/// @param[in] max How many buckets do we want to get? If not enough have been
///                written we will get less.
/// @param[in] backOffset Offset from last to m_current
/// @param[out] first first (older) bucket
/// @param[out] last last (newest) bucket
/// Note that both first and last are valid buckets, so
/// if max==1, then first=last
/// 
bool CircularPosition::last(
				Position max, int back, Position & first, Position & last) const
{
	omni_mutex_lock lock(this->m_mutex);
	ensure(max <= (this->m_maxPos - this->m_minPos)); // Full range - 1(current)

	if(!this->m_hasBegun) {
		first = this->m_minPos;
		last = this->m_minPos;
		return false;
	}
	
	Position lp =  this->move(this->m_current, -back);

	// totalRange = How many buckets do we want? >> 1 + |last - first|
	Position totalRange = max - 1;
	if(!this->m_hasLooped) {
		Position dist = this->distance(lp, this->m_first);
		if (dist == 0)
			return false;
		if (dist < max)
			totalRange = dist - 1;
	}
	
	if(lp == this->m_minPos)
		last = this->m_maxPos;
	else
		last = lp - 1;

	if(totalRange > last - this->m_minPos)
		first = last + (this->m_maxPos + 1) - totalRange;
	else
		first = last - totalRange;

	return true;
}
