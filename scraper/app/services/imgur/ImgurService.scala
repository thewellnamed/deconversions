package services.imgur

import akka.actor._
import akka.contrib.throttle._
import akka.contrib.throttle.Throttler._
import akka.routing.SmallestMailboxPool
import play.api.Logger
import play.api.libs.ws.WSClient
import play.api.libs.json._
import javax.inject._
import scala.concurrent.duration._

import net.ruippeixotog.scalascraper.browser.JsoupBrowser
import net.ruippeixotog.scalascraper.dsl.DSL._
import net.ruippeixotog.scalascraper.dsl.DSL.Extract._
import net.ruippeixotog.scalascraper.dsl.DSL.Parse._
import net.ruippeixotog.scalascraper.model.Element

import models._

case class DataRequest(pages: Int)
case class PageRequest(page: Int)
case class ImageMetaRequest(id: String)
case class ImageRequest(url: String)
case class RedditRequest(meta: ImageMeta)
case class DownloadRequest(id: Long, img: Image, failures: Int)

case class ImageMetaSuccess(request: ImageMetaRequest, data: ImageMeta)
case class RedditSuccess(meta: ImageMeta, thread: JsValue)
case class DownloadSuccess(id: Long, img: Image, data: Array[Byte])
case class DownloadFailure(request: DownloadRequest)

object ImgurService {
  def props = Props[ImgurService]
}

class ImgurService @Inject()(ws: WSClient, dao: ImgurDAO) extends Actor {
  import ImgurService._
  
  val metaWorker = context.actorOf(ImgurWorker.props(ws))
  val throttledMetaWorker = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 1.second))
  throttledMetaWorker ! SetTarget(Some(metaWorker))
  
  val downloadWorkers = context.actorOf(SmallestMailboxPool(5).props(routeeProps = ImgurWorker.props(ws)))
  val throttledDownload = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 1.second))
  throttledDownload ! SetTarget(Some(downloadWorkers))
  
  val throttledSelf = context.actorOf(Props(classOf[TimerBasedThrottler], 1 msgsPer 60.second))
  throttledSelf ! SetTarget(Some(self))
  
  def receive = {
    case DataRequest(pages) => {
      (0 to pages).foreach { i =>
        throttledSelf ! PageRequest(i)
      }
    }
    
    case PageRequest(page) => {
      Logger.info("fetching: http://imgur.com/r/mensrights/new/page/" + page)
      
      val browser = JsoupBrowser()
      val doc = browser.get("http://imgur.com/r/mensrights/new/page/" + page)
        
      // extract image links
      val ids = doc >?> elementList(".posts .post")
      
      if (ids.isDefined) {
        ids.get.foreach { id =>
          throttledMetaWorker ! ImageMetaRequest(id.attr("id"))
        } 
      } else {
        Logger.info("No posts found for page " + page)
      }
    }
    
    case r: ImageMetaRequest => {
      throttledMetaWorker ! r
    }
    
    case ImageMetaSuccess(request, data) => {
      throttledMetaWorker ! RedditRequest(data)
    }
    
    case RedditSuccess(meta, thread) => {
      val id = dao.addMeta(ImageMeta(meta.id, meta.key, meta.title, meta.discussionURL, Some(thread), meta.images))
      
      if (id > 0) {
        meta.images.foreach { img => 
          throttledDownload ! DownloadRequest(id, img, failures = 0)  
        }
      }
    }
    
    case DownloadSuccess(id, img, data) => {
      dao.addImage(id, img, data)
    }
    
    case DownloadFailure(req) => {
      Logger.warn("download failure: image=" + req.img.filename + ", failures=" + req.failures)
      if (req.failures < 3) {
        throttledDownload ! DownloadRequest(req.id, req.img, req.failures + 1)
      }
    }
      
  } 
}