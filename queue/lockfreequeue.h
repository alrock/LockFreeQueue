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

	bool is_empty() const
		{ return first->state == node_base::Unready; }

	size_type size() const;

	void produce(pointer v) {
		node *p = make_empty_node();
		last->next = p;
		fill_node(last, v);
		last = p;
	}

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
