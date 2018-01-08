#pragma once

namespace SL {
namespace NET {
    template <typename T> void append(T **head, T *context)
    {
        if (!*head) {
            *head = context;
        }
        else {
            (*head)->prev = context;
            context->next = *head;
            *head = context;
        }
    }
    template <typename T> void remove(T **head, T *context)
    {
        if (context == *head) {
            if (context->next) { // more elemenets in the list
                context->next->prev = nullptr;
                *head = context->next;
            } // no more elements
            else {
                *head = nullptr;
            }
        }
        else if (!context->next) { // not the head.  the last element so at least two elements are in this list
            if (context->prev) {
                context->prev->next = nullptr;
            }
        }
        else if (context->prev) { // middle element at least three in the list
            context->prev->next = context->next;
        }
        context->next = context->prev = nullptr; // make sure to cleanup any links
    }
    template <typename T> T *pop_front(T **head)
    {
        auto tmp = *head;
        if (!tmp) {
            return tmp;
        }
        else if (!tmp->next) { // just the head, no other elements
            *head = nullptr;
            tmp->next = tmp->prev = nullptr;
            return tmp;
        }
        else { // more than one element
            *head = tmp->next;
            tmp->next->prev = nullptr;
            tmp->next = tmp->prev = nullptr;
        }
        return tmp;
    }
} // namespace NET
} // namespace SL