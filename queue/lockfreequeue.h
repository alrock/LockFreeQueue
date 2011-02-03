#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <memory>

/**
  * @brief Базовый класс для элемента односвязного списка
  */
template <typename Atomic> struct ListNodeBase_ {
	/// Перечисление задаёт возможные состояния элемента
	enum State { Ready, Unready };

	Atomic state_;
	ListNodeBase_<Atomic> *next;

	/// Обменивает 2 элемента местами
	static void swap(const ListNodeBase_<Atomic>& x, const ListNodeBase_<Atomic>& y);
};

/**
  * @brief Информационная единица, используется как базовый элемент списка
  * Добален указатель на предыдущий элемент, чтобы не искать его постоянно.
  */
template <typename Atomic> struct ListNodeEnd_: public ListNodeBase_<Atomic> {
	ListNodeBase_<Atomic> *prev;
};

/**
  * @brief Собственно элемент односвязного списка, несущий информацию
  */
template <typename T, typename Atomic> struct ListNode_: public ListNodeBase_<Atomic> {
	T data_;
};

/**
  * @brief LockFreeQueue::iterator.
  *
  * Это не совсем обычный итератор. Он не умеет ходить назад, а самое страшное -
  * он циклический.
  */
template<typename T, typename Atomic> struct ListIterator_ {
	typedef ListIterator_<T, Atomic>           Self_;
	typedef ListNode_<T, Atomic>               Node_;
	typedef ListNodeBase_<Atomic>              ListNodeBaseType_;

	typedef ptrdiff_t                          difference_type;
	typedef std::bidirectional_iterator_tag    iterator_category;
	typedef T                                  value_type;
	typedef T*                                 pointer;
	typedef T&                                 reference;

	ListIterator_(): base_node(), first_node() { }

	explicit ListIterator_(ListNodeBaseType_ *x): base_node(x), first_node(x) {}

	reference operator * () const
		{ return static_cast<Node_*>(base_node)->data_; }

	pointer operator -> () const
		{ return &static_cast<Node_*>(base_node)->data_; }

	Self_& operator ++ () {
		if (base_node->next->state_ == ListNodeBaseType_::Ready)
			base_node = base_node->next;
		else base_node = first_node;

		return *this;
	}

	Self_& operator ++ (int) {
		Self_ tmp = *this;

		if (base_node->next->state_ == ListNodeBaseType_::Ready)
			base_node = base_node->next;
		else base_node = first_node;

		return tmp;
	}

	bool is_valid() const
		{ return base_node->state_ == ListNodeBaseType_::Ready; }

	// Назад в односвязном списке ходить нельзя
	// Self_& operator -- ();

	bool operator == (const Self_& x) const
		{ return base_node == x.base_node; }

	bool operator != (const Self_& x) const
		{ return base_node != x.base_node; }

	ListNodeBaseType_ *base_node;
	ListNodeBaseType_ *first_node;
  };

/**
  * @brief Базовый класс для односвязного списка
  */
template<typename T, typename Atomic, typename Alloc> class ListBase_ {
protected:
	typedef ListNodeBase_<Atomic> ListNodeBaseType_;

	typedef ListNodeEnd_<Atomic>  ListNodeEndType_;

	typedef ListNode_<T, Atomic>  ListNodeType_;

	typedef typename Alloc::template rebind<ListNode_<T, Atomic> >::other NodeAllocType_;

	typedef typename Alloc::template rebind<T>::other TAllocType_;

	typedef typename Alloc::template rebind<Atomic>::other AtomicAllocType_;

	struct ListImpl_: public NodeAllocType_ {
		ListNodeEndType_ base_node_;

		ListImpl_(): NodeAllocType_(), base_node_() {}
		ListImpl_(const NodeAllocType_ &a): NodeAllocType_(a), base_node_() {}
	};

	ListImpl_ base_impl_;

	ListNodeType_* get_node()
		{ return base_impl_.NodeAllocType_::allocate(1); } // Как?

	void put_node(ListNodeType_ *p)
		{ base_impl_.NodeAllocType_::deallocate(p, 1); }

public:
	typedef Alloc AllocatorType;

	NodeAllocType_& getNodeAllocator()
		{ return *static_cast<NodeAllocType_*>(&this->base_impl_); }

	const NodeAllocType_& getNodeAllocator() const
		{ return *static_cast<const NodeAllocType_*>(&this->base_impl_); }

	TAllocType_ getTAllocator() const
		{ return TAllocType_(getNodeAllocator()); }

	AtomicAllocType_ getAtomicAllocator() const
		{ return AtomicAllocType_(getNodeAllocator()); }

	AllocatorType getAllocator() const
		{ return AllocatorType(getNodeAllocator()); }

	ListBase_(): base_impl_()
		{ base_init(); }

	ListBase_(const AllocatorType& a): base_impl_(a)
		{ base_init(); }

	~ListBase_()
		{ base_clear(); }

	/**
	  * @brief Полностью очищает список, удаляя из него все элементы
	  * Чтобы восстановить работоспособность списка после применения этой
	  * функции вызывайте %base_init
	  */
	void base_clear() {
		ListNodeBaseType_ *p = this->base_impl_.base_node_.next;
		if (p == p->next)
			return;
		// По ходу это можно и быстрее проделать, но да ладно
		ListNodeType_ *t = static_cast<ListNodeType_*>(p);
		while (t != (ListNodeBaseType_*)&this->base_impl_.base_node_) {
			getAtomicAllocator().destroy(&t->state_);
			if (t->state_ != ListNodeBaseType_::Unready) {
				getTAllocator().destroy(&t->data_);
			}
			ListNodeType_ *d = t;
			t = static_cast<ListNodeType_*>(t->next);
			this->put_node(d);
		}
	}

	void base_init() {
		this->base_impl_.base_node_.next = &this->base_impl_.base_node_;
		this->base_impl_.base_node_.prev = &this->base_impl_.base_node_;
		this->base_impl_.base_node_.state_ = ListNodeBaseType_::Unready;
	}
};

template <typename T, typename Atomic = int, typename Alloc = std::allocator<T> >
class LockFreeQueue: protected ListBase_<T, Atomic, Alloc> {

	typedef typename Alloc::value_type              AllocValueType_;
	typedef ListBase_<T, Atomic, Alloc>             Base_;
	typedef typename Base_::TAllocType_	            TAllocType_;
	typedef typename Base_::ListNodeBaseType_       ListNodeBaseType_;
	typedef typename Base_::ListNodeEndType_        ListNodeEndType_;

public:
	typedef T                                       value_type;
	typedef typename TAllocType_::pointer           pointer;
	typedef typename TAllocType_::const_pointer     const_pointer;
	typedef typename TAllocType_::reference         reference;
	typedef typename TAllocType_::const_reference   const_reference;

	typedef ListIterator_<T, Alloc>                 iterator;

	typedef size_t                                  size_type;
	typedef ptrdiff_t                               difference_type;
	typedef Alloc                                   allocator_type;

protected:
	typedef ListNode_<T, Atomic>				    Node_;

	using Base_::base_impl_;
	using Base_::put_node;
	using Base_::get_node;
	using Base_::getTAllocator;
	using Base_::getAtomicAllocator;
	using Base_::getNodeAllocator;

	Node_* create_empty_node() {
		Node_ *p = this->get_node();
		try {
			getAtomicAllocator().construct(&p->state_,
										   Atomic(ListNodeBaseType_::Unready));
		} catch(...) {
			put_node(p);
			// throw again
			throw;
		}
		return p;
	}

	void fill_empty_node(Node_ *p, const value_type& x) {
		try {
			getTAllocator().construct(&p->data_, x);
		} catch(...) {
			// TODO Нужно мне что-либо предпринимать?
			throw;
		}
		p->state_ = ListNodeBaseType_::Ready;
	}

public:
	/// Default constructor creates no elements.
	LockFreeQueue(): Base_() { initialize_(); }

	/**
	 *  Creates a %LockFreeQueue with no elements.
	 *  @param  a  An allocator object.
	 */
	explicit LockFreeQueue(const allocator_type& a): Base_(a) { initialize_(); }

	/**
	 *  Creates a %LockFreeQueue with copies of an exemplar element.
	 *  @param  n  The number of elements to initially create.
	 *  @param  value  An element to copy.
	 *  @param  a  An allocator object.
	 *
	 *  This constructor fills the %LockFreeQueue with @a n copies of @a value.
	 */
	explicit LockFreeQueue(size_type n, const value_type& value = value_type(),
						   const allocator_type& a = allocator_type()): Base_(a)
		{ fill_initialize_(n, value); }

	/**
	 *  %LockFreeQueue copy constructor.
	 *  @param  x  A %LockFreeQueue of identical element and allocator types.
	 *
	 *  The newly-created %LockFreeQueue uses a copy of the allocation object used
	 *  by @a x.
	 */
	LockFreeQueue(const LockFreeQueue& x): Base_(x.getAllocator())
		{ initialize_copy_(x); }

	// TODO Здесь должна быть ещё парочка конструкторов

	/**
	 *  %LockFreeQueue assignment operator.
	 *  @param  x  A %LockFreeQueue of identical element and allocator types.
	 *
	 *  All the elements of @a x are copied, but unlike the copy
	 *  constructor, the allocator object is not copied.
	 */
	LockFreeQueue& operator = (const LockFreeQueue& x) {
		clear();
		initialize_copy_(x);

		return *this;
	}

	/// Будут проблемы с пустым списком
	iterator begin()
		{ return iterator(this->base_impl_.base_node_.next); }

	/// Get a copy of the memory allocation object.
	allocator_type get_allocator() const
		{ return Base_::getAllocator(); }

	bool empty() const {
		return this->base_impl_.base_node_.next->state_ == ListNodeBaseType_::Unready;
	}

	/**
	  * @brief Возвращает количество элементов доступных для извлечения.
	  * Функция возвращает адекватный результат только если не производятся
	  * операции извлечения в других потоках.
	  * Используется простой перебор, т.е. сложность O(n), старайтесь не
	  * использовать эту функцию, по крайней мере пока её кто-нибудь не исправит.
	  */
	size_type size() const {
		if (empty()) return 0;
		// Очень лениво мне как-то ускорять этот процесс, так что считаем размер
		// полным перебором.
		const Node_ *p = static_cast<Node_* const>(this->base_impl_.base_node_.next);

		for (size_type i = 0; ; ++i) {
			if (p->state_ == ListNodeBaseType_::Unready)
				return i;
			p = static_cast<Node_*>(p->next);
		}
		return 0;
	}

	/**
	  * @brief Возвращает ссылку на первый элемент очереди.
	  * Потокобезопасно с точки зрения очереди.
	  */
	reference front() {
		return static_cast<Node_*>(this->base_impl_.base_node_.next)->data_;
	}

	const_reference front() const {
		return static_cast<Node_*>(this->base_impl_.base_node_.next)->data_;
	}

	/**
	  * @brief Извлекает из очереди и возвращает первый элемент.
	  * Потокобезопасна по отношению к другим операциям только если вызывается
	  * в одном потоке.
	  */
	value_type take_front() {
		if (empty()) {
			; // Че-то надо возбудить
		}
		Node_ *p = static_cast<Node_*>(this->base_impl_.base_node_.next);
		// Делаю копию данных и уничтожаю элемент
		value_type x = p->data_;
		erase_(p);
		return x;
	}

	void pop_front() {
		if (empty())
			return;
		Node_ *p = static_cast<Node_*>(this->base_impl_.base_node_.next);
		erase_(p);
	}

	/**
	  * Добавляет новый элемент в конец очереди.
	  * Потокобезопасна по отношению к другим операциям только если вызывается
	  * в одном потоке.
	  */
	void push_back(const value_type& x) {
		Node_ *p = create_empty_node();
		Node_ *n = static_cast<Node_*>(this->base_impl_.base_node_.prev);

		p->next = &this->base_impl_.base_node_;
		this->base_impl_.base_node_.prev = p;
		n->next = p;
		fill_empty_node(n, x);
	}

	/**
	  * @brief Полная очистка очереди
	  * Функция не потокобезопасна при любых вариантах использования. Если
	  * необходимо просто разгрузить очередь, воспользуйтесь безопасной для
	  * для записи %extract_all.
	  */
	void clear() {
		this->base_clear();
		this->base_init();
		initialize_();
	}

	/**
	  * @brief Разгрузка очереди
	  * Извлекает из очереди все элементы. Безопасна по отношению к операциям
	  * записи.
	  */
	void extract_all()
		{ while (!empty()) pop_front(); }

protected:
	void initialize_() {
		if (this->base_impl_.base_node_.next == &this->base_impl_.base_node_) {
			Node_ *p = create_empty_node();

			p->next = &this->base_impl_.base_node_;
			this->base_impl_.base_node_.prev = this->base_impl_.base_node_.next = p;
		}
	}

	void fill_initialize_(size_type n, const value_type& value) {
		initialize_();

		for (size_type i = 0; i < n; ++i)
			push_back(value);
	}

	void initialize_copy_(const LockFreeQueue& x) {
		initialize_();

		if (x.empty())
			return;

		Node_ *p = static_cast<Node_*>(x.base_impl_.base_node_.next);
		while (p->state_ != ListNodeBaseType_::Unready) {
			push_back(p->data_);
			p = static_cast<Node_*>(p->next);
		}
	}

	void erase_(Node_ *p) {
		this->base_impl_.base_node_.next = p->next;

		this->getTAllocator().destroy(&p->data_);
		this->getAtomicAllocator().destroy(&p->state_);
		this->put_node(p);
	}
};

#endif // LOCKFREEQUEUE_H
