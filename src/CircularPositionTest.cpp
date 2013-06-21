
#include <QObject>
#include <QtTest/QtTest>
#include "CircularPosition.h"


class CircularPositionTest: public QObject
{
    Q_OBJECT
private slots:
    	void initTestCase()
    	{ 
		qDebug("called before everything else");
	}

	void normalFlowTest()
	{
		CircularPosition c;
		CircularPosition::Position p1, p2, first=1;
		
		/// Configuration phase
		c.set_limits(0, 5);
		c.starts_with(first);
		
		QVERIFY(!c.current(p1));
		QVERIFY(!c.last(p1));
		QVERIFY(!c.last(3, p1, p2));
		
		/// Begin:
		c.go_next();
		// The first bucket has been selected to
		// write into, but the writting is not yet
		// done. So current() = first, but last()
		// is still not valid
		QVERIFY(c.current(p1));
		QCOMPARE(p1, first);
		QVERIFY(!c.last(p1));
		QVERIFY(!c.last(3, p1, p2));
		
		/// First acquisition done
		c.go_next();
		// Now we have selected the second bucket
		// for writing, so the first one is
		// the only one now available as last()
		QVERIFY(c.current(p1));
		QCOMPARE(p1, first + 1);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, first);
		QVERIFY(c.last(3, p1, p2));
		QCOMPARE(p1, first);
		QCOMPARE(p2, first);
	}
	void mySecondTest()
	{
		CircularPosition c;
		CircularPosition::Position p1, p2, first=0;
		
		// Configuration phase
		c.set_limits(0, 5);
		c.starts_with(first);
		
		/// Begin:
		c.go_next();
		/// First acquisition done
		c.go_next();
		QVERIFY(c.current(p1));
		QCOMPARE(p1, 1);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, 0);
		QVERIFY(c.last(3, p1, p2));
		QCOMPARE(p1, 0);
		QCOMPARE(p2, 0);
		
		/// See normalFlowTest for what I've been doing until here
		
		/// Second acquisition done
		c.go_next();
		QVERIFY(c.current(p1));
		QCOMPARE(p1, 2);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, 1);
		QVERIFY(c.last(3, p1, p2));
		QCOMPARE(p1, 0);
		QCOMPARE(p2, 1);
		
		/// Third acquisition done
		c.go_next();
		QVERIFY(c.current(p1));
		QCOMPARE(p1, 3);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, 2);
		// Now I can take the last 3 acquisitions
		QVERIFY(c.last(3, p1, p2));
		QCOMPARE(p1, 0);
		QCOMPARE(p2, 2);
		// Or only the last 2 acquisitions
		QVERIFY(c.last(2, p1, p2));
		QCOMPARE(p1, 1);
		QCOMPARE(p2, 2);
		
		/// Fifth acquisition done
		c.go_next();
		c.go_next();
		QVERIFY(c.current(p1));
		QCOMPARE(p1, 5);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, 4);
		// Now I can take the last 3 acquisitions
		QVERIFY(c.last(3, p1, p2));
		QCOMPARE(p1, 2);
		QCOMPARE(p2, 4);
		// Or only the last 2 acquisitions
		QVERIFY(c.last(2, p1, p2));
		QCOMPARE(p1, 3);
		QCOMPARE(p2, 4);
		// Or last 5 acquisitions
		QVERIFY(c.last(5, p1, p2));
		QCOMPARE(p1, 0);
		QCOMPARE(p2, 4);
		
		/// Sixth acquisition done
		c.go_next();
		QVERIFY(c.current(p1));
		QCOMPARE(p1, 0);
		QVERIFY(c.last(p1));
		QCOMPARE(p1, 5);
		// Or last 5 acquisitions
		QVERIFY(c.last(5, p1, p2));
		QCOMPARE(p1, 1);
		QCOMPARE(p2, 5);
		// Or last 6 acquisitions
		bool exc = false;
		try	{
			c.last(6, p1, p2);
		} catch(...) { exc = true; }
		QVERIFY(exc);
	}
	void cleanupTestCase()
	{
		qDebug("called after myFirstTest and mySecondTest"); 
	}
};

QTEST_MAIN(CircularPositionTest)
#include "CircularPositionTest.moc"

