package services.imgur

import akka.actor._
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

import models._

import net.ruippeixotog.scalascraper.browser.JsoupBrowser
import net.ruippeixotog.scalascraper.dsl.DSL._
import net.ruippeixotog.scalascraper.dsl.DSL.Extract._
import net.ruippeixotog.scalascraper.dsl.DSL.Parse._
import net.ruippeixotog.scalascraper.model.Element
import net.ruippeixotog.scalascraper.model.Document

object ImgurWorker {
  def props(ws: WSClient): Props = Props(classOf[ImgurWorker], ws)
}

class ImgurWorker (ws: WSClient) extends Actor {
  import ImgurWorker._
  
  def receive = {
    case request: ImageMetaRequest => {
      val browser = JsoupBrowser()
      val doc = browser.get("http://imgur.com/r/mensrights/" + request.id)
      
      val title = doc >?> text(".post-title")
      val discussionURL = doc >?> attr("href")(".post-title-meta a")
      
      val imageList = doc >?> elementList(".post-images .post-image-container") 
      
      if (title.isDefined && discussionURL.isDefined && imageList.isDefined) {
        val images = imageList.get.map { img =>
          val id = img.attr("id")
          val posted = img >> attr("content")("meta")
          val imgSource = img >> attr("src")(".post-image img")
          val url = new URL("http:" + imgSource)
          val filename = url.getPath().substring(1)
          
          var postedDate: Instant = null
          try {
            postedDate = LocalDate.parse(posted, DateTimeFormatter.ISO_LOCAL_DATE).atStartOfDay(ZoneOffset.UTC).toInstant()  
          }
          catch {
            case e: Exception => Unit
          }
        
          Image(0, id, filename, postedDate)
        }
          
        sender ! ImageMetaSuccess(request, ImageMeta(0, request.id, title.get, discussionURL.get, None, images))
      } else {
        Logger.info("skipped image_meta for " + request.id)
      }
    }
    
    case request: RedditRequest => {
      val browser = JsoupBrowser()
      val doc = browser.get(request.meta.discussionURL)
      val threads = extractThreads(doc, None)
      
      sender ! RedditSuccess(request.meta, threads)
    }
    
    case request: DownloadRequest => {
      val receiver = sender
      
      val response: Future[WSResponse] = ws.url(s"http://i.imgur.com/" + request.img.filename)
          .withRequestTimeout(10.seconds)
          .get()

      response.map { r =>
        if (r.status == 200) {
          receiver ! DownloadSuccess(request.id, request.img, r.bodyAsBytes.toArray[Byte])
        } else {
          receiver ! DownloadFailure(request)
        }
      }.recover {
        case e: Exception => sender ! DownloadFailure(request)
      }

      Await.result(response, 11.seconds)
    }
  }
  
  def extractThreads(doc: Document, postId: Option[String]): JsValue = {
    val threads = postId match {
      case Some(id) => doc >?> elementList("#" + id + " > .child > .sitetable > .comment[id*=thing]")
      case None => doc >?> elementList(".commentarea > .sitetable > .comment[id*=thing")
    }
                
    val data = threads match {
      case Some(t) => {
        t.map { thread =>
          val postId = thread.attr("id")
          val username = thread >> text(".tagline .author")
          val post = thread >> element(".usertext-body .md")
      
          Json.obj(
                "user" -> username,
                "text" -> post.innerHtml,
                "children" -> extractThreads(doc, Some(postId)))
        }
      }
      case None => List.empty[JsValue]  
    }
    
    Json.toJson(data)
  }
}