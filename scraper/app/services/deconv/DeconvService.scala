package services.deconv

import akka.actor._
import akka.contrib.throttle._
import akka.contrib.throttle.Throttler._
import akka.routing.SmallestMailboxPool
import play.api.Logger
import play.api.libs.ws.WSClient
import play.api.libs.json._
import javax.inject._
import scala.concurrent.duration._

import java.time._
import java.time.format.DateTimeFormatter

import net.ruippeixotog.scalascraper.browser.JsoupBrowser
import net.ruippeixotog.scalascraper.model.Document
import net.ruippeixotog.scalascraper.dsl.DSL._
import net.ruippeixotog.scalascraper.dsl.DSL.Extract._
import net.ruippeixotog.scalascraper.dsl.DSL.Parse._
import net.ruippeixotog.scalascraper.model.Element

import models._

case class DataRequest(baseUrl: String)
case class PageRequest(baseUrl: String, page: Int)
case class ThreadRequest(parent: ActorRef, thread: Thread, baseUrl: String, page: Int)
case class ThreadData(thread: Thread, page: Int, posts: List[Post])

case class StatsUpdateRequest()
case class FetchStatsData(offset: Int)
case class ProcessStatsRequest(posts: List[PostForStats], offset: Int)
case class ProcessStatsResponse(posts: List[PostForStats], words: List[WordEntry])

case class TitleStatsRequest()

case class PhrasesRequest()
case class FetchPhrasesData(offset: Int)
case class ProcessPhrasesRequest(posts: List[PostForStats], offset: Int)
case class ProcessPhrasesResponse(posts: List[PostForStats], phrases: List[WordEntry])

object DeconvService {
  def props = Props[DeconvService]
}

class DeconvService @Inject()(ws: WSClient, dao: DeconvDAO) extends Actor {
  import DeconvService._
  
  val worker = context.actorOf(DeconvWorker.props)
  val throttledWorker = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 3.second))
  throttledWorker ! SetTarget(Some(worker))
  
  val throttledSelf = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 60.second))
  throttledSelf ! SetTarget(Some(self))
  
  val throttledStats = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 60.second))
  throttledStats ! SetTarget(Some(self))  
  
  def receive = {
    case DataRequest(url: String) => {
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
      
      Logger.info("last page: " + lastPage)
      processPage(doc)
      
      if (lastPage > 1) {
        (2 to lastPage).foreach { page => throttledSelf ! PageRequest(url, page) }
      }
    }
    
    case PageRequest(baseUrl: String, page: Int) => {
      val url = baseUrl + "?page=" + page;
      Logger.info("fetching: " + url)
      
      val browser = JsoupBrowser()
      val doc = browser.get(url)
      processPage(doc)
    }
    
    case data: ThreadData => dao.addPosts(data.thread, data.page, data.posts)
    
    case StatsUpdateRequest => {
      val totalUpdateCount = dao.getCountOfPostsWithoutStats()
      val pages = if (totalUpdateCount % 1000 != 0) { (totalUpdateCount/1000 + 1) } else { totalUpdateCount/1000 }
      
      Logger.info("processing stats: " + totalUpdateCount + " posts...")
      
      (1 to pages).foreach { page => 
        val offset = (page - 1) * 1000
        throttledStats ! FetchStatsData(offset)
      }
    }
    
    case FetchStatsData(offset: Int) => {
        val posts = dao.getPostsForStatsUpdate(1000)
        throttledWorker ! ProcessStatsRequest(posts, offset)
    }
    
    case data: ProcessStatsResponse => dao.updateStats(data.posts, data.words)
    
    case TitleStatsRequest => {
      // get thread titles
      // do work
      // insert
    }
  }
  
  def processPage(doc: Document) = {
    val topics = doc >?> elementList(".cTopicList > .ipsDataItem")
    val threadIdPattern = "http://www.ex-christian.net/topic/([0-9]+)-.*".r
    val authorIdPattern = "http://www.ex-christian.net/profile/([0-9]+)-.*".r
    
    if (topics.isDefined) {
      topics.get.foreach { topic => 
        val url = topic >> attr("href")(".ipsDataItem_main a")
        val title = topic >> text(".ipsDataItem_main a span")
        val threadIdPattern(origThreadId) = url
        
        val authorElem = topic >> element(".ipsDataItem_meta span")
        val author = authorElem.text.substring(3, authorElem.text.length - 1)
        val authorUrl = authorElem >?> attr("href")("a")
        
        val origAuthorId = authorUrl match {
          case Some(url) => url match {
            case authorIdPattern(id) => id.toInt
            case _ => 0
          }
          case None => 0
        }

        val posted = topic >> attr("datetime")("time")
        var postedDate: Instant = null
        try {
          postedDate = Instant.parse(posted) 
        }
        catch {
          case e: Exception => Logger.info("parse: " + e.getMessage)
        }
        
        val authorId = dao.getAuthorId(origAuthorId, author)
        
        val threadData = Thread(
            threadId = 0,
            originalId = origThreadId.toInt,
            title = title,
            url = url, 
            authorId = authorId,
            originalAuthorId = origAuthorId.toInt,
            created = postedDate)
            
        val thread = dao.addThread(threadData)
        
        if (thread.isDefined) {
          throttledWorker ! ThreadRequest(self, thread.get, url, 1)
        } else {
          Logger.warn("Unable to save thread: " + threadData.title)
        }
      }
    }
  }
}