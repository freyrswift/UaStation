# Automation Entry Points

Recommended opening sequence:

1. `js-reverse_new_page` or `js-reverse_navigate_page` to open the page
2. `js-reverse_list_network_requests` to see recent requests
3. `js-reverse_get_request_initiator` to find the call stack
4. `js-reverse_list_scripts` to establish the script scope
5. `js-reverse_search_in_sources` to search request paths, parameter names, function names
6. When needed, `js-reverse_break_on_xhr` or `js-reverse_set_breakpoint_on_text`

By default, do not start by guessing how `window`, `document`, `navigator` should be patched.
