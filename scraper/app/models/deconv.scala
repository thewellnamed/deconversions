package models

import java.time._
import play.api.libs.json._

case class Thread(threadId: Int, originalId: Int, title: String, url: String, authorId: Int, originalAuthorId: Int, created: Instant)
case class Post(postId: Int, originalId: Int, threadId: Int, authorId: Int, originalAuthorId: Int, author: String, created: Instant, text: String)
case class Author(authorId: Int, originalId: Int, name: String)

case class WordEntry(threadId: Int, posterId: Int, postId: Int, wordType: Int, word: String, count: Int)
case class PostForStats(postId: Int, posterId: Int, threadId: Int, threadAuthor: Int, seq: Int, text: String)