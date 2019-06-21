#ifndef functor_h
#define functor_h


namespace tp {

    /*
     * We cannot use std::function for task storation
     * because we need to find a container to store the std::packaged_task.
     * however, the std::function requires its stored object to be copy-constructable
     * while std::packaged_task is only movable.
     * so we need to invent a container to handle the move-only objects.
     */
    class functor {
    private:
        struct impl_base {
            virtual void call()=0;
            virtual ~impl_base() {}
        };

        std::unique_ptr <impl_base> impl;

        template<typename F, typename ...Args>
        struct impl_type : impl_base {
            F f;

            impl_type(F && _f) : f(std::move(_f)) {}

            void call(Args && ... ags) { f(std::forward<Args>(ags)...); }
        };

    public:
        template<typename F>
        functor(F &&f)
                :impl(new impl_type<F>(std::move(f))) {}
        template <typename ...Args>
        void operator()(Args && ...ags) { impl->call(std::forward<Args>(ags)...); }

        functor() = default;

        functor(functor &&other) :

                impl(std::move(other.impl)) {}

        functor &operator=(functor &&other) {
            impl = std::move(other.impl);
            return *this;
        }

        functor(const functor &) = delete;

        functor(functor &) = delete;

        functor &operator=(const functor &)= delete;

    };
}

#endif /* functor_h */


