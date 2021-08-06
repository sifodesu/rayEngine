#pragma once
template <class T>
class Locator {
public:
    static T& get()
        return *_service;
    static void provide(T* service)
        _service = service;
private:
    static T* _service;
};

template<class T> T Locator::_service;