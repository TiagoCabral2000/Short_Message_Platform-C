# TP_SO_2425
Platform for sending and receiving short messages, organized by topics

## Features

### User registration on the platform ✅
- Feed: username indicated in the feed command line (./feed username)
- Manager: maximum registration of 10 users. Repeated usernames are not accepted. Returns feedback to the user on the success of their registration

### Topic creation
Any user can send messages to a specific topic. If the topic doesn’t exist and the server’s maximum topic limit hasn’t been reached, the topic will be automatically created, allowing users to subscribe to it afterward. Topics are identified by their name (e.g., “football,” “technology,” “movies,” etc.), and it’s assumed that each topic name is a single word

### Message sending
Whenever a user sends a message (using their feed) to a topic, the message will be delivered and immediately displayed to all online users who have subscribed to that topic

### Types of message
There are two types of messages: non-persistent and persistent. < br / >Non-persistent messages are not stored; once the manager receives them, they are distributed to users interested in the topic and then discarded. Persistent messages are also distributed to interested users but are stored by the manager for a certain period (referred to as the “lifetime” of the message). During this time, the messages are delivered to any users who connect to the platform and subscribe to the topic of these messages (or, if already online, subscribe to the topic later). Each persistent message has its own lifetime, which may differ across messages. The user sending the message must specify the message duration (in seconds) when sending it. If the specified duration is 0, the message will be treated as non-persistent. Once the specified lifetime expires, the manager discards the message.
Upon startup, the manager loads any persistent messages still within their lifetime from a text file (implicitly creating the respective topics). For these messages, the timer continues without resetting their remaining lifetime. Further details are provided later.
