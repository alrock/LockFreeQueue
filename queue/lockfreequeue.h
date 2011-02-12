#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <memory>
#include <QAtomicInt>

/**
  * @brief Базовый класс для элемента односвязного списка
  */
struct ListNodeBase_ {
	/// Перечисление задаёт возможные состояния элемента
	enum State { Ready, Unready };

	QAtomicInt state;
	ListNodeBase_ *next;
};

/**
  * @brief Собственно элемент односвязного списка, несущий информацию
  */
template <typename T> struct ListNode_: public ListNodeBase_ {
	T data;
};

/**
  * @brief Producer/Consumer wait-free queue
  * Очередь предназначенная для работы между двумя потоками, один из которых
  * добавляет в неё элементы, другой читает. Как добавление, так и чтение может
  * производиться только в одном потоке.
  * Шаблонный параметр T в силу лени разработчиков должен быть типом, который
  * не являеся ни указателем, ни ссылкой. В очереди все данные хранятся по
  * указателю, но всё-же возможно добавлять данные через перегруженный метод
  * produce, который принимает константную ссылку и снимает копию данных для
  * хранения в очереди.
  */
template <typename T> class waitfree_queue {
protected:
	typedef ListNodeBase_             node_base;
	typedef waitfree_queue<T>         queue_type;
	typedef T&                        reference;
	typedef const T&                  const_reference;
	typedef T*                        pointer;

	typedef ListNode_<pointer>        node;

	typedef size_t                    size_type;

	node* make_empty_node() {
		node *p = new node;

		p->next = 0;
		p->state.fetchAndStoreOrdered(node_base::Unready);

		return p;
	}

	node* fill_node(node *n, pointer v) {
		n->data = v;
		n->state.fetchAndStoreOrdered(node_base::Ready);

		return n;
	}

public:
	waitfree_queue()
		{ initialize(); }
	waitfree_queue(const queue_type &q)
		{ copy_initialize(q); }

	waitfree_queue& operator = (const queue_type &q) {
		clear();
		copy_initialize(q);
	}

	/**
	  * @brief Проверка на пустоту
	  * Безопасна для вызова только из потока, производящего извлечение.
	  */
	bool is_empty() const
		{ return first->state == node_base::Unready; }

	/// Не реализовано
	size_type size() const;

	/**
	  * @brief Добавляет данные в конец очереди
	  * Этот метод должен вызываться только в одном потоке. В этом случае он
	  * безопасен относительно операции извлечения.
	  */
	void produce(pointer v) {
		node *p = make_empty_node();
		last->next = p;
		fill_node(last, v);
		last = p;
	}

	/**
	  * @brief Извлекает данные из очереди
	  * Этот метод должен вызываться только в одном потоке. В этом случае он
	  * безопасен относительно операции добавления
	  */
	pointer consume() {
		if (first->state == node_base::Ready) {
			pointer p = first->data;
			first = static_cast<node*>(first->next);
			return p;
		}
		return 0;
	}

private:
	node *first;
	node *last;

	void clear() {
		while (this->consume());
	}

	void initialize() {
		node *base = make_empty_node();
		first = last = base;
	}

	void copy_initialize(const queue_type &q) {
		initialize();

		node const *p = q.first;
		while (p->state == node_base::Ready) {
			this->produce(p->data);
			p = p->next;
		}
	}
};

#endif // LOCKFREEQUEUE_H
