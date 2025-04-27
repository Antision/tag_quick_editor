#include"pch.h"

namespace ants {
template<typename T>
void printtype() {
    using namespace std;
    using RT = remove_reference_t<T>;
    if (is_const_v<RT>)
        cout << "const ";
    cout << typeid(T).name();
    if (is_reference_v<T>)
        cout << "&";
    if (is_rvalue_reference_v<T>)
        cout << "&";
    cout << endl;
}
template<typename T>
struct node {
    T value;
    node* next;
};
template<typename T>
struct pair {
    int id;
    node<T>* address;
};
template<typename T, int index_lenth = 4>
class skiplist {
public:
    pair<T> index[index_lenth];
    node<T>* _begin;
    node<T>* _end;
    using iterator = node<T>*;
    iterator begin() {
        return _begin;
    }
    iterator end() {
        return _end;
    }
    const iterator begin() const {
        return _begin;
    }
    const iterator end() const {
        return _end;
    }
    int size = 0;
    skiplist() {
        _begin = (node<T>*)::operator new(sizeof(node<T>));
        _end = _begin;
        memset((void*)index, 0, sizeof(pair<T>) * index_lenth);
    }

    void refresh_index()
    {
        if (size < 2) {
            for (int i = 0; i < index_lenth; ++i) {
                index[i].address = _end;
                index[i].id = size;
            }
        }
        else if (size < (index_lenth + 1) * 2) {
            for (int i = 2; i < size + 1; i += 2) {
                pair<T>& idx_ = index[i / 2 - 1];
                idx_.address = &(*this)[i];
                idx_.id = i;
            }
            for (int i = size / 2; i < index_lenth; ++i) {
                index[i].address = _end;
                index[i].id = size;
            }
        }
        else {
            int interval = (size + 1) / (index_lenth + 1);
            for (int i = interval, j = 0; j < index_lenth; i += interval, ++j) {
                pair<T>& idx_ = index[j];
                idx_.address = &(*this)[i];
                idx_.id = i;
            }
        }
    }
    void rearrange_index() {
        if (size < (index_lenth + 1) * 2)
        {
            refresh_index();
            return;
        }
        else
        {
            int avg_interval = (size + 1) / (index_lenth + 1), max_deviation = 1;
            if ((abs(index[0].id - avg_interval) > max_deviation) ||
                (abs(size - index[index_lenth - 1].id - avg_interval) > max_deviation))
            {
                refresh_index();
                return;
            }
            for (int i = 0; i < index_lenth - 1; ++i)
                if (abs(index[i + 1].id - index[i].id - avg_interval) > max_deviation || index[i + 1].id < index[i].id)
                {
                    refresh_index();
                    return;
                }
        }
    }
    template<typename I>
    void insert(I&& th, int pos = 0) {
        if (pos == 0 || pos == size) {
            _end->next = new node<T>{ std::forward<I>(th), nullptr };
            _end = _end->next;
        }
        else
        {
            node<T>* newnode = new node<T>{ std::forward<I>(th), nullptr };
            newnode->next = (*this)[pos].next;
            (*this)[pos].next = newnode;
            for (int i = 0; i < index_lenth; ++i) {
                if (index[i].id > pos) {
                    for (int j = i; j < index_lenth; ++j) {
                        ++index[j].id;
                    }
                }
            }
        }
        ++size;
        rearrange_index();
    }
    void erase(int pos = 0) {
        if (pos < 1)pos += size;
        if (pos == size) {
            (*this)[size - 1].next = nullptr;
            delete _end;
            _end = &(*this)[size - 1];
            --size;
            rearrange_index();
            return;
        }
        node<T>* deletenode = &(*this)[pos];
        (*this)[pos - 1].next = (*this)[pos].next;
        for (int i = 0; i < index_lenth; ++i) {
            if (index[i].id > pos) {
                for (int j = i; j < index_lenth; ++j) {
                    --index[j].id;
                }
                break;
            }
            else if (index[i].id == pos) {
                index[i].address = deletenode->next;
                for (int j = i + 1; j < index_lenth; ++j) {
                    --index[j].id;
                }
                break;
            }
        }
        delete deletenode;
        --size;
        rearrange_index();
    }
    void clear(){
        while(size>0)
            erase(0);
    }
    T&& take(int pos = 0) {
        if (pos < 1)pos += size;
        T tmp = std::move((*this)[size - 1].value);
        erase(pos);
        return std::move(tmp);
    }
    void debug_point() {
        system("cls");
        for (int i = 0; i < index_lenth; ++i) {
            for (int j = 0; j < index[i].id; ++j)
                printf(" ");
            printf("%d\n", i);
        }
        for (int i = 0; i < (size + 1); ++i) {
            printf("+");
        }
        printf("\n");
        node<T>* tmpnode = _begin;
        for (int i = 0; i < size; ++i) {
            tmpnode = tmpnode->next;
            printf("%d ", tmpnode->value);
        }
        printf("\n");
        for (int i = 0; i < index_lenth; ++i) {
            printf("%d:%d:%p ", index[i].id, index[i].address->value, index[i].address);
        }
        printf("\n");
    }
    node<T>& operator[](int pos) {
        if (index[index_lenth - 1].id < pos) {
            node<T>* position_node = index[index_lenth - 1].address;
            for (int d = pos - index[index_lenth - 1].id; d > 0; --d) {
                position_node = position_node->next;
            }
            return *position_node;
        }
        else if (index[0].id > pos) {
            node<T>* position_node = _begin;
            for (int d = pos; d > 0; --d) {
                position_node = position_node->next;
            }
            return *position_node;
        }
        for (int i = 0; i < index_lenth; ++i) {
            if (index[i].id != 0)
            {
                if (index[i].id == pos)
                    return *(index[i].address);
                else if (index[i].id<pos && index[i + 1].id>pos) {
                    node<T>* position_node = index[i].address;
                    for (int d = pos - index[i].id; d > 0; --d) {
                        position_node = position_node->next;
                    }
                    return *position_node;
                }
                else continue;
            }
            else {
                node<T>* position_node = _begin;
                for (int d = pos; d > 0; --d) {
                    position_node = position_node->next;
                }
                return *position_node;
            }
        }
    }
};

template<typename T> requires (!requires { T(); })
constexpr size_t return_type_size = 0;
template<typename T, typename ...Args>
constexpr size_t return_type_size<T(Args...)> = sizeof(T);
template<typename ...Args>
constexpr size_t return_type_size<void(Args...)> = 1;

struct task_base {
    task_base() = default;
    virtual void operator()() {
        throw std::exception("calling task_base operator()");
    }
    virtual ~task_base() {
    }
    virtual void* value() { return nullptr; };
};
template<typename T, typename...Args>
struct task :virtual task_base {
    using type = T;
    task() = default;

    template<typename...inputArgs> requires requires(T(F)(Args...)) {
            std::bind(F, std::forward<inputArgs>(std::declval<std::remove_reference_t<inputArgs>>())...);
        }
    task(T(func)(Args...), inputArgs&&...args)
        :binder(std::bind(func, std::forward<inputArgs>(args)...))
    {
        if constexpr (!std::is_same_v<T, void>)
            memset(value_data, 0, sizeof(T));
    }
    template<typename L, typename...inputArgs>
        requires (std::is_same_v<decltype(std::declval<L>().operator()(
                                               std::declval<std::remove_reference_t<inputArgs>>()...)), T > )
    task(L&& func, inputArgs&&...args)
        :binder(std::bind(func, std::forward<inputArgs>(args)...))
    {
        if constexpr (!std::is_same_v<T, void>)
            memset(value_data, 0, sizeof(T));
    }

    std::function<T(void)> binder;
    void operator()() {
        if constexpr (!std::is_same_v<void, type>)
            new(value_data) type{ binder() };
        else binder();
    }
    void* value() {
        return value_data;
    }
    ~task() {
        ((type*)value_data)->~type();
    }
protected:
    uint8_t value_data[return_type_size<T(Args...)>];
};
template<typename T, typename...Args>
task(T lambda, Args&&...args) -> task<decltype(lambda(std::forward<Args>(args)...)), Args...>;
template<typename T, int _capacity = 16>
struct queue {
    std::mutex mt;
    std::condition_variable_any cd_full, cd_empty;
    constexpr static int capacity = _capacity;
    T queue_arr[_capacity];
    int begin = 0;
    int end = 0;
    int size() {
        if (begin > end) return _capacity - begin + end;
        else return end - begin;
    }
    void insert(std::initializer_list<T> in_list) {
        for (const T& e : in_list) {
            T* ptr = const_cast<T*>(&e);
            insert(std::move(*ptr));
        }
    }
    template<typename in> requires (std::is_same_v<std::remove_reference_t<in>, T>
                 || requires(std::remove_reference_t<in> A, T * ptr) {
                        ptr = &A;
                    } || requires(std::remove_reference_t<in> A, T ptr) {
                        ptr = &(*A);
                    })
    void insert(in&& i) {
        std::unique_lock tmp_unique_lock(mt);
        if ((end + 1) % _capacity == begin) {
            cd_full.wait(tmp_unique_lock);
        }
        queue_arr[end] = std::forward<in>(i);
        ++end %= _capacity;
        cd_empty.notify_one();
    }
    void drop(bool ifdestroy = true) {
        bool if_unlock = false;
        std::unique_lock<std::mutex>tmp_unique_lock(mt);
        if (begin == end) {
            cd_empty.wait(tmp_unique_lock);
        }
        if (ifdestroy)
            queue_arr[begin].~T();
        ++begin %= _capacity;
        cd_full.notify_one();
    }
    T take() {
        std::unique_lock<std::mutex> tmp_unique_lock(mt);
        if (begin == end)
            cd_empty.wait(tmp_unique_lock);
        ++begin %= _capacity;
        cd_full.notify_one();
        return std::move(queue_arr[(begin - 1 + _capacity) % _capacity]);
    }
};
typedef class ThreadPool
{
public:
    ThreadPool() {
        workers.insert(new worker(this));
        manager = std::thread(&ThreadPool::manager_function, this);
        ++thread_count;
    }
    std::mutex continue_mt;
    std::condition_variable_any continue_cd;
    queue<task_base*> taskqueue;
    int thread_count = 0;
    int max_thread_count=8;
    std::atomic<int> working_count = 0;
    std::mutex delete_count_mt;
    std::atomic<int> delete_count = 0;
    std::thread manager;
    struct worker {
        worker() = delete;
        worker(ThreadPool* in_tp) :parent(in_tp) {
            work_thread = std::thread(&worker::worker_function, this);
        }
        worker(worker&& tmp) = delete;
        std::thread work_thread;
        ThreadPool* parent;
        bool delete_flag = false;
        std::mutex delete_flag_mt;
        std::atomic<bool> finished = false;
        bool waiting = false;
        void worker_function() {
            while (true) {
                if (parent->delete_count > 0) {
                    std::unique_lock<std::mutex>(delete_count_mt);
                    if (parent->delete_count > 0)
                    {
                        --parent->delete_count;
                        break;
                    }
                }
                else {
                    parent->continue_mt.lock();
                    delete_flag_mt.lock();
                    if (delete_flag)
                    {
                        finished = true;
                        parent->continue_mt.unlock();
                        parent->continue_cd.notify_one();
                        delete_flag_mt.unlock();
                        return;
                    }
                    delete_flag_mt.unlock();
                    while(true)
                    {
                        if (parent->taskqueue.size() == 0)
                            parent->continue_cd.wait(parent->continue_mt);
                        else break;
                        delete_flag_mt.lock();
                        if (delete_flag)
                        {
                            finished = true;
                            parent->continue_mt.unlock();
                            parent->continue_cd.notify_one();
                            delete_flag_mt.unlock();
                            return;
                        }
                        delete_flag_mt.unlock();
                    }
                    task_base* t = parent->taskqueue.take();
                    parent->continue_mt.unlock();
                    waiting = false;
                    ++parent->working_count;
                    t->operator()();
                    delete t;
                    waiting = true;
                    --parent->working_count;
                }
            }
            finished = true;
            return;
        }
        bool detach(){
            if(work_thread.joinable()){
                delete_flag=true;
                work_thread.detach();
                return true;
            }else
                return false;
        }
        ~worker() {
            if(work_thread.joinable())
                work_thread.join();
        }
    };
    skiplist<worker*> workers;
    void terminate(){
        for (int i = 1; i <= workers.size;++i) {
            workers[i].value->detach();
        }
    }
    ~ThreadPool() {
        manager_run = false;
        manager.detach();
        for (int i = 1; i <= workers.size;++i) {
            workers[i].value->delete_flag_mt.lock();
            workers[i].value->delete_flag = true;
            workers[i].value->delete_flag_mt.unlock();
        }
        for (int i = 1; i <= workers.size; ++i) {
            continue_cd.notify_all();
            delete workers[i].value;
        }
    }
    std::mutex manager_function_mt;
    std::condition_variable_any manager_function_cd;
    bool manager_run=true;
    void manager_function() {
        int if_reduce_count = 0;
        while (manager_run)
        {
            for (int i = 1; i < workers.size + 1; ++i) {
                if (workers[i].value->finished) {
                    delete workers[i].value;
                    workers.erase(i);
                    --thread_count;
                }
                else {
                    std::unique_lock<std::mutex>tmp_unique_lock(delete_count_mt);
                    if (workers[i].value->waiting && delete_count > 0)
                    {
                        workers[i].value->delete_flag=true;
                        continue_cd.notify_all();
                        delete workers[i].value;
                        workers.erase(i);
                        --delete_count;
                        --thread_count;
                    }
                }
            }
            if (workers.size > 3 && (double)working_count / (double)thread_count < 0.7)
            {
                if (++if_reduce_count > 2)
                    delete_count = (int)(0.3 * thread_count);
            }
            else {
                if_reduce_count = 0;
                while ((thread_count<=max_thread_count)&&((taskqueue.capacity - taskqueue.size() < 2) || (working_count == thread_count && taskqueue.size() > 0))) {
                    workers.insert(new worker(this));
                    ++thread_count;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            manager_function_mt.lock();
            manager_function_cd.wait_for(manager_function_mt, std::chrono::seconds(2));
            manager_function_mt.unlock();
        }
    }
    template<typename T, typename...Args>
    void add(T func, Args&&...args) {
        taskqueue.insert(new task(func, std::forward<Args>(args)...));
        manager_function_cd.notify_one();
        continue_cd.notify_one();
    }

}thpl;
}
extern ants::thpl thread_pool;
