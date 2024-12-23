package services.deconv

import akka.actor._
import akka.contrib.throttle._
import akka.contrib.throttle.Throttler._
import play.api.Logger
import play.api.libs.ws.{WSClient, WSRequest, WSResponse}
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import java.net.URL
import java.time._
import java.time.format.DateTimeFormatter
import javax.inject._
import scala.concurrent.duration._
import scala.concurrent.{Await, Future}
import scala.collection.JavaConversions._

import models._
import models.fathom._

import net.ruippeixotog.scalascraper.browser.JsoupBrowser
import net.ruippeixotog.scalascraper.dsl.DSL._
import net.ruippeixotog.scalascraper.dsl.DSL.Extract._
import net.ruippeixotog.scalascraper.dsl.DSL.Parse._
import net.ruippeixotog.scalascraper.model.Element
import net.ruippeixotog.scalascraper.model.Document

object DeconvWorker {
  def props = Props[DeconvWorker]
}

class DeconvWorker extends Actor {
  import DeconvWorker._
  
  val throttledSelf = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 1.second))
  throttledSelf ! SetTarget(Some(self))
  
  def receive = {
    case req: ThreadRequest => {
      val url = req.page match {
        case 1 => req.baseUrl
        case _ => req.baseUrl + "?page=" + req.page
      }
      
      Logger.info("fetching: " + url)
      
      val browser = JsoupBrowser()
      val doc = browser.get(url)
      
      val pageText = doc >?> text(".ipsPagination_pageJump")
      val pagesPattern = "Page 1 of ([0-9]+).*".r
      
      val lastPage = pageText match {
        case Some(pt) => {
          pt match {
            case pagesPattern(pageNum) => pageNum.toInt
            case _ => 1
          }
        }
        
        case None => 1
      }
      
      val postElems = doc >> elementList(".ipsComment_content")
      
      val posts = postElems.map { p =>
        val jsonData = Json.parse(p.attr("data-quotedata"))
        val comment = (p >> element(".ipsType_richText")).innerHtml
        val posted = p >> attr("datetime")("time")
        var postedDate: Instant = null
        try {
          postedDate = Instant.parse(posted) 
        }
        catch {
          case e: Exception => Logger.info("parse: " + e.getMessage)
        }
        
        val originalAuthorId = (jsonData \ "userid").get match {
          case i: JsNumber => i.as[Int]
          case _ => 0
        }
        
        Post(postId = 0, 
             originalId = (jsonData \ "contentcommentid").as[Int],
             threadId = req.thread.threadId,
             authorId = 0,
             originalAuthorId = originalAuthorId,
             author = (jsonData \ "username").as[String],
             created = postedDate,
             text = comment)
      }
      
      if (lastPage > 1) {
        (2 to lastPage).foreach { page => throttledSelf ! ThreadRequest(req.parent, req.thread, req.baseUrl, page) }
      }
           
      req.parent ! ThreadData(req.thread, req.page, posts)  
    }
    
    case req: ProcessStatsRequest => {
      val words: scala.collection.mutable.ListBuffer[WordEntry] = new scala.collection.mutable.ListBuffer[WordEntry]()
      val commentBrowser = JsoupBrowser.typed()
      
      Logger.info("process stats: " + " (" + req.posts.length + " posts)")
      
      req.posts foreach { post =>
        // strip quotes, get text blocks
        val commentDoc = commentBrowser.parseString("<div id=\"deconv\">" + post.text + "</div>")
        val commentElem = commentDoc >> pElement("#deconv")
        commentElem.underlying.children.select("blockquote").remove()
        val sections = commentElem >> elementList("p")
        
        // 1 == OP
        // 2 == response from OP
        // 3 == Other posters
        val postType = if (post.posterId == post.threadAuthor) { 
          if (post.seq == 1) { 1 } else { 2 }
        } else { 3 }
        
        sections.foreach { section =>
          val s = Fathom.analyze(section.text)

          s.getUniqueWords() foreach { case (w, count) =>
            val word = w.toLowerCase()
            
            if (word.length > 1 && word.length < 21 && !PorterStemmer.stopWords.contains(word)) {
              val stem = PorterStemmer.stem(word)
              val we = WordEntry(post.threadId, post.posterId, post.postId, postType, stem, count)
              words.append(we)
            }
          }
        }
      }
      
      Logger.info("response: " + words.length + " entries")
      sender ! ProcessStatsResponse(req.posts, words.toList)
    }    
  }
}