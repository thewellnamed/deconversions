package services.deconv

import anorm._
import anorm.SqlParser._
import play.api.db._
import play.api.Logger
import play.api.Environment
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import scala.concurrent.duration._
import scala.concurrent.Future
import javax.inject._
import java.time.Instant
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import models._
import models.account.Account

class DeconvDAO @Inject()(db: Database, env: Environment)
{
  val posters: scala.collection.mutable.Map[Int, Int] = new scala.collection.mutable.HashMap[Int, Int]()
  posters(0) = 1 // guest
  
  val postStatsParser = {
    get[Int]("post_id") ~
    get[Int]("poster_id") ~
    get[Int]("thread_id") ~
    get[Int]("thread_poster_id") ~
    get[Int]("seq") ~
    get[String]("content") map {
      case postId ~ posterId ~ threadId ~ authorId ~ seq ~ content => 
        PostForStats(postId, posterId, threadId, authorId, seq, content)
    }
  }  
  
  def getAuthorId(originalId: Int, name: String): Int = {
    db.withConnection { implicit c =>
      
      if (posters.contains(originalId)) {
        return posters(originalId)
      }
      
      val existing = SQL("SELECT poster_id FROM posters WHERE orig_id={originalId}")
          .on('originalId -> originalId)
          .as(SqlParser.int("poster_id").singleOpt)
          
      if (existing.isDefined) {
        posters(originalId) = existing.get
        return existing.get
      } else {
        val id = SQL("INSERT INTO posters (name, orig_id) VALUES ({name}, {originalId})")
           .on('name -> name, 'originalId -> originalId)
           .executeInsert()
           
        id match {
          case Some(l) => {
            posters(originalId) = l.toInt
            return l.toInt
          }
          
          case None => return 0
        }
      }
    }
  }
  
  def addThread(t: Thread): Option[Thread] = {
    db.withConnection { implicit c =>
      val existing = SQL("SELECT thread_id FROM threads WHERE orig_id={originalId}")
          .on('originalId -> t.originalId)
          .as(SqlParser.int("thread_id").singleOpt)
      
      existing match {
        case Some(id) => Some(Thread(id, t.originalId, t.title, t.url, t.authorId, t.originalAuthorId, t.created))
        case None => {
          val id = SQL("""INSERT INTO threads (orig_id, poster_id, title, url, created) 
                            VALUES ({originalId}, {posterId}, {title}, {url}, {created})""")
           .on('originalId -> t.originalId, 
               'posterId -> t.authorId,
               'title -> t.title,
               'url -> t.url,
               'created -> t.created)
           .executeInsert()
          
          id match {
            case Some(l) => Some(Thread(l.toInt, t.originalId, t.title, t.url, t.authorId, t.originalAuthorId, t.created))
            case None => None
          }          
        }
      }  
    }
  }
  
  def addPosts(thread: Thread, page: Int, posts: List[Post]) = {
    db.withConnection { implicit c =>
      var seq = ((page - 1) * 25)  + 1;
      
      posts.foreach { p =>
        val authorId = getAuthorId(p.originalAuthorId, p.author)
        SQL("DELETE FROM posts WHERE comment_id={originalId}")
            .on('originalId -> p.originalId)
            .executeUpdate()
            
        SQL("""INSERT INTO posts (thread_id, poster_id, comment_id, seq, created, content) 
               VALUES ({threadId}, {authorId}, {commentId}, {seq}, {created}, {content})""")
           .on('threadId -> thread.threadId,
               'authorId -> authorId,
               'commentId -> p.originalId,
               'seq -> seq,
               'created -> p.created,
               'content -> p.text)
           .executeInsert()
      
        seq += 1
      }
    }
  }
  
  def getCountOfPostsWithoutStats(): Int = {
    db.withConnection { implicit c =>
      val total = SQL("SELECT count(post_id) AS posts FROM posts WHERE has_stats=0")
          .as(SqlParser.int("posts").single)
          
      total
    }
  }
  
  def getPostsForStatsUpdate(limit: Int): List[PostForStats] = {
    db.withConnection { implicit c =>
      Logger.info("fetch next stats batch")
      
      SQL("""SELECT p.post_id, p.poster_id, p.thread_id, t.poster_id AS thread_poster_id, p.seq, p.content 
             FROM posts p
             JOIN threads t ON (t.thread_id=p.thread_id) 
             WHERE p.has_stats=0 ORDER BY p.post_id LIMIT {limit}""")
        .on('limit -> limit)
        .as(postStatsParser.*)
    }
  }
  
  def updateStats(posts: List[PostForStats], words: List[WordEntry]) = {
    db.withConnection { implicit c => 
      Logger.info("-----> process stats for chunk")
      posts.foreach { p =>
        SQL("UPDATE posts SET has_stats=1 WHERE post_id={postId}")
            .on('postId -> p.postId)
            .executeUpdate()
      }
      
      //process words
      val types = words.map { w => w.wordType } distinct
      val threads = words.map { w => w.threadId } distinct
      
      types.foreach { wt =>
        threads.foreach { t =>
          val wordList = words.iterator
              .filter(w => w.wordType == wt)
              .filter(w => w.threadId == t)
              .toList
              
          val uniqueWords = wordList.map { we => we.word } distinct
          
          uniqueWords.foreach { w =>
            val count = wordList.filter(we => we.word.equals(w)).foldLeft(0)((count, we) => count + we.count)
            val posts = (wordList.filter(we => we.word.equals(w)).map { we => we.postId } distinct).length
            val posters = (wordList.filter(we => we.word.equals(w)).map { we => we.posterId } distinct).length
            
            val existing = SQL("""SELECT count(*) as count 
                                  FROM words 
                                  WHERE type={type} AND thread_id={threadId} AND word={word}""")
              .on('type -> wt, 'threadId -> t, 'word -> w)
              .as(SqlParser.int("count").single)
              
            if (existing > 0) {            
              SQL("""UPDATE words 
                     SET count = count + {count}, posts = posts + {posts}, posters = posters + {posters}
                     WHERE type={type} AND thread_id={threadId} AND word={word}""")
                .on('type -> wt,
                    'threadId -> t,
                    'word -> w, 
                    'count -> count,
                    'posts -> posts,
                    'posters -> posters)
                .executeUpdate()
            } else {
              SQL("""INSERT INTO words (type, thread_id, word, count, posts, posters)
                     VALUES ({type}, {threadId}, {word}, {count}, {posts}, {posters})""")
                .on('type -> wt,
                    'threadId -> t,
                    'word -> w, 
                    'count -> count,
                    'posts -> posts,
                    'posters -> posters)
                .executeInsert(SqlParser.scalar[String].single)
            }
          }
        }
      }
      
      Logger.info("-----> finished stats update for chunk...")
    }
  }  
}