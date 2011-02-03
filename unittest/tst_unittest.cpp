#include <QtCore/QString>
#include <QtTest/QtTest>
#include "../queue/lockfreequeue.h"

class UnitTest : public QObject {
    Q_OBJECT
public:
    UnitTest();

private Q_SLOTS:
    void simple_test();
	void different_types_test();
	void cover_all_methods();
	void assign_operator_test();
	void speed_test();
	void std_speed_test();
	void async_test();
};

UnitTest::UnitTest() { }

/**
  * @brief Простой тест, проверяет базовые функции очереди.
  * Проверяется правильность добавления и извлечения элементов.
  */
void UnitTest::simple_test() {
	LockFreeQueue<int> l;

	LockFreeQueue<int>::size_type size = 100;
	for (int i = 0; i < size; ++i) {
		l.push_back(i);
	}
	QCOMPARE(l.size(), size);
	for (int i = 0; i < size; ++i)
		QCOMPARE(l.take_front(), i);
	QVERIFY(l.empty());
}

/**
  * @brief Тест проверяет работоспособность с разными типами.
  * Создаются очереди под строки и очередь очередей, проверяются конструкторы
  * и некоторые методы.
  */
void UnitTest::different_types_test() {
	LockFreeQueue<QString, char> l1;

	l1.push_back(QString("The world"));
	l1.push_back(QString("is so"));
	l1.push_back(QString("beautiful"));

	QCOMPARE(l1.take_front(), QString("The world"));
	QCOMPARE(l1.take_front(), QString("is so"));
	QCOMPARE(l1.take_front(), QString("beautiful"));

	LockFreeQueue<LockFreeQueue<int> > l2;

	l2.push_back(LockFreeQueue<int>(100, 84365834));
	l2.push_back(LockFreeQueue<int>(200, 123456));


	for (int i = 0; i < 100; ++i) {
		QCOMPARE(l2.front().take_front(), 84365834);
	}
	QVERIFY(l2.front().empty());
	l2.pop_front();
	for (int i = 0; i < 200; ++i) {
		QCOMPARE(l2.front().take_front(), 123456);
	}
	QVERIFY(l2.front().empty());
	l2.pop_front();
	QVERIFY(l2.empty());
}

/**
  * @brief Проверяет методы не проверенные в предыдущих тестах
  * Проверяет очистку и разгрузку очереди.
  */
void UnitTest::cover_all_methods() {
	LockFreeQueue<double> l;

	l.push_back(1.3224);
	l.push_back(3.14);
	l.push_back(2.7);

	QVERIFY(l.size() == 3);

	l.clear();
	QVERIFY(l.empty());
	QVERIFY(l.size() == 0);

	l.push_back(10.345);
	QVERIFY(l.size() == 1);

	l.push_back(15.6);
	l.extract_all();
	QVERIFY(l.empty());
	QVERIFY(l.size() == 0);

	l.push_back(2.77);
	QCOMPARE(l.front(), 2.77);
	QVERIFY(l.size() > 0);
}


/**
  * @brief Проверяет оператор присваивания.
  */
void UnitTest::assign_operator_test() {
	LockFreeQueue<int> l1, l2(50, 12345), l3(60, 5678), l4(70, 98765);
	l1 = l2;
	for (int i = 0; i < 50; ++i) {
		QCOMPARE(l1.take_front(), l2.take_front());
	}
	QVERIFY(l1.empty());
	QVERIFY(l2.empty());

	l3 = l4;
	QVERIFY(l3.size() == l4.size());
	for (int i = 0; i < 70; ++i) {
		QCOMPARE(l3.take_front(), l4.front());
	}
	QVERIFY(l3.empty());
	QVERIFY(l4.size() == 70);
}

#include <list>

void UnitTest::speed_test() {
	LockFreeQueue<int> l;
	QBENCHMARK {
		for (int i = 0; i < 1000000; ++i)
			l.push_back(i);
		for (int i = 0; i < 1000000; ++i) {
			l.front();
			l.pop_front();
		}
	}
}

void UnitTest::std_speed_test() {
	std::list<int> s;
	QBENCHMARK {
		for (int i = 0; i < 1000000; ++i)
			s.push_back(i);
		for (int i = 0; i < 1000000; ++i) {
			s.front();
			s.pop_front();
		}
	}
}

#include <QRunnable>
#include <QThreadPool>

class AsyncTest: public QRunnable {
public:
	LockFreeQueue<int> *q;
	bool io;
	int count;

	void run() {
		if (io) {
			int i = 0;
			while (i != count) {
				if (!q->empty()) {
					QCOMPARE(q->take_front(), i);
					++i;
				}
			}
			for (int i = 0; i < count; ++i) {
				if(!q->empty()) q->take_front();
			}
		} else {
			for (int i = 0; i < count; ++i) {
				q->push_back(i);
			}
		}
	}
};

void UnitTest::async_test() {
	AsyncTest *a1 = new AsyncTest, *a2 = new AsyncTest;
	LockFreeQueue<int> l;
	a1->q = a2->q = &l;
	a1->io = false;
	a2->io = true;
	a1->count = a2->count = 1000000;

	QThreadPool::globalInstance()->start(a2);
	QThreadPool::globalInstance()->start(a1);

	QThreadPool::globalInstance()->waitForDone();

	QCOMPARE(l.size(), (unsigned)0);
	QVERIFY(l.empty());
}

QTEST_APPLESS_MAIN(UnitTest);

#include "tst_unittest.moc"
