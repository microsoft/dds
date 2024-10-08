/*
 * Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License
 */

#pragma once

template <typename T>
struct NodeT {
    T Value;
    NodeT* Next;
};

template <typename T>
class LinkedList {
private:
    NodeT<T>* Head;
    NodeT<T>* Tail;

public:
    //
    // template class implementation must
    // be put in the header file
    //
    //
    LinkedList();
    ~LinkedList();
    void Insert(T NodeValue);
    void Delete(T NodeValue);
    void DeleteAll();
    NodeT<T>* Begin();
    NodeT<T>* End();
};