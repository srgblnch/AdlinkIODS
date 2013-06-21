
#ifndef __CIRCULAR_POSITION_H____JDFfdfjFJDJdfFDJDFJ390390fg__
#define __CIRCULAR_POSITION_H____JDFfdfjFJDJdfFDJDFJ390390fg__

#include <omnithread.h>

///
/// The purpose of this class is to complement a 'SharedBuffer'.
/// SharedBuffer accepts random write access and has no knowledge
/// about always circular access. circular position manages which
/// is the next bucket to be written, remembers which ones have
/// been already written and so have a meaning... And offer ways
/// to the readers to know this information.
///
class CircularPosition
{
	// see .cpp for documentation of the functions...
public:
	typedef int Position;
	
private:
	Position m_minPos;
	Position m_maxPos;
	
	Position m_first;
	Position m_current;
	bool m_hasLooped;
	bool m_hasBegun;
	
	mutable omni_mutex m_mutex;
	
	// no thread-safe, not public.
	// Can't be thread-safed bc its called from the others...
	int distance(Position big, Position low) const;
	Position move(Position pos, int dist) const;
	
public:
	CircularPosition();

	void set_limits(Position min, Position max);
	void starts_with(Position p);
	void stop();
	Position go_next();
	
	bool current(Position &cur) const;
	Position current() const;

	Position get_next(Position c) const;
	Position get_prev(Position c) const;
	
	bool last(Position & buck) const;
	bool last(Position max, Position & first, Position & last) const;
	bool last(Position max, int backOffset, Position & first, Position & last) const;
};

#endif //__CIRCULAR_POSITION_H____JDFfdfjFJDJdfFDJDFJ390390fg__
