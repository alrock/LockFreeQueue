#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QRunnable>
#include <QThreadPool>
#include "../queue/lockfreequeue.h"

class UnitTest : public QObject {
    Q_OBJECT
public:
    UnitTest();

private Q_SLOTS:

	/**
	  * @brief Проверка поточного добавления/извлечения на коллизии
	  */
	void collision_test();
};

UnitTest::UnitTest() { }

struct Item {
	int a;
	bool b;

	Item(int x, bool y): a(x), b(y) {}
};

class Producer: public QRunnable {
public:
	waitfree_queue<Item> *q;
	QVector<Item*> items;
	int count;

	void run() {
		try {
			qsrand(100500);
			items.clear();
			for (int i = 0; i < count; ++i) {
				Item *i = new Item(qrand(), qrand() > RAND_MAX/2);
				items.append(i);
				q->produce(i);
			}
		} catch (...) {
			qDebug() << "Ooops in producer";
			throw;
		}
	}
};

class Consumer: public QRunnable {
public:
	waitfree_queue<Item> *q;
	QVector<Item*> items;
	int count;

	void run() {
		try {
			items.clear();
			int i = 0;
			while (i != count) {
				Item *p = q->consume();
				if (p) {
					items.append(p);
					++i;
				}
				//QThread::currentThread()->msleep(10);
			}
		} catch (...) {
			qDebug() << "Ooops in consumer";
			throw;
		}
	}
};

void UnitTest::collision_test() {
	waitfree_queue<Item> q;
	Producer p;
	p.setAutoDelete(false);
	Consumer c;
	c.setAutoDelete(false);
	p.q = c.q = &q;

	QTime timer;
	timer.start();

	for (int i = 10000; i < 500000; i+= 1000) {
		p.count = c.count = i;
		QThreadPool::globalInstance()->start(&p);
		QThreadPool::globalInstance()->start(&c);

		QThreadPool::globalInstance()->waitForDone();

		QVERIFY(q.is_empty());

		QCOMPARE(p.items.size(), c.items.size());
		QCOMPARE(p.items.size(), p.count);

		for (int j = 0; j < p.count; ++j) {
			QCOMPARE(p.items[j], c.items[j]);
			QCOMPARE(p.items[j]->a, c.items[j]->a);
			QCOMPARE(p.items[j]->b, c.items[j]->b);
			delete p.items[j];
		}
	}
	qDebug() << timer.elapsed();
}

QTEST_APPLESS_MAIN(UnitTest);

#include "tst_unittest.moc"
