# Extension Analysis Essentials

| Field | Risk signal |
|------|----------|
| host_permissions `<all_urls>` | Can read/write any site |
| webRequestBlocking | Man-in-the-middle style rewriting |
| nativeMessaging | Exits the browser onto the local machine |
| externally_connectable | Web page drives the extension |

MV3: watch the service_worker lifecycle and declarativeNetRequest.
