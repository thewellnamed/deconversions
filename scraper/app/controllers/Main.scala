package controllers

import play.api.mvc._
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import play.api.libs.json.Json._
import play.filters.csrf._
import play.filters.csrf.CSRF.Token
import play.api.Environment
import play.Logger
import akka.actor._
import akka.pattern.ask
import akka.routing._
import akka.util.Timeout
import javax.inject._
import scala.concurrent.{ExecutionContext,TimeoutException}
import scala.concurrent.duration._
import scala.concurrent.Future
import com.mohiva.play.silhouette.api.Silhouette
import com.mohiva.play.silhouette.api.actions.SecuredErrorHandler

import models._
import models.account._
import models.Implicits._
import services.account.{AccountEnv, AccountService}
import services.imgur.ImgurDAO
import utils.Implicits.jsonWriteAccount

class Main @Inject()(
  env: Environment, 
  imgurDAO: ImgurDAO,
  acctService: AccountService,
  silhouette: Silhouette[AccountEnv]) extends Controller
{
  val errorHandler = new SecuredErrorHandler {
    override def onNotAuthenticated(implicit request: RequestHeader) = {
      Future.successful(Unauthorized)
    }
    override def onNotAuthorized(implicit request: RequestHeader) = {
      Future.successful(Forbidden)
    }
  }   
  
  def renderWithOptions(view: String, index: Long, imageId: Long) = Action { implicit request =>
    val csrfToken = CSRF.getToken.get
    Ok(views.html.index(view, index, imageId, csrfToken))
  }
  
  def home = renderWithOptions(view = "browse", index = 0, imageId = 0)
  def viewIndex(idx: Long) = renderWithOptions(view = "code", index = idx, imageId= 0)
  def viewImage(id: Long) = renderWithOptions(view = "code", index = 0, imageId = id) 
  def login = renderWithOptions(view = "login", index = 0, imageId = 0)
   
  def fetchImages(m: String, c: String) = silhouette.UserAwareAction.async { implicit request =>
    val metaCodes = m.isEmpty match {
      case true => List.empty[Int]
      case false => m.split(',').map(_.toInt).distinct.toList
    }
    val codes = c.isEmpty match {
      case true => List.empty[Int]
      case false => c.split(',').map(_.toInt).distinct.toList
    }
    
    for {
      imageData <- request.identity match {
        case Some(a) => imgurDAO.fetchWithCodes(metaCodes, codes)
        case None => imgurDAO.fetchAll()
      }
      accounts  <- acctService.fetchAll()
    } yield {
      request.identity match {
        case Some(a) => Ok(Json.obj("results" -> imageData, "accounts" -> Json.toJson(accounts.map { a => a.id.toString -> a.name } toMap)))
        case None => Ok(Json.obj("results" -> imageData))
      }      
    }
  }
  
  def fetchImageDetails(imageId: Int) = silhouette.UserAwareAction.async { implicit request =>
    if (request.identity.isDefined) {
      for {
        codes <- imgurDAO.fetchCodes(imageId, request.identity.get.id)
        comments <- imgurDAO.fetchComments(imageId)
        thread <- imgurDAO.fetchThread(imageId)
      } yield {
        Ok(Json.obj(
            "success" -> true, 
            "codes" -> codes,
            "comments" -> comments, 
            "thread" -> thread))
      } 
    } else {
      imgurDAO.fetchThread(imageId).map { thread =>
        Ok(Json.obj("success" -> true, "thread" -> thread))
      }
    }
  }
  
  def fetchCodes(imageId: Int) = silhouette.SecuredAction(errorHandler).async { implicit request =>
    imgurDAO.fetchCodes(imageId, request.identity.id).map { codes =>
      Ok(Json.obj("success" -> true, "data" -> codes))
    } recover {
      case e: Exception => {
        Logger.error("unable to fetch image codes: " + e.getMessage())
        Ok(Json.obj("success" -> false))
      }
    }
  }
  
  def fetchComments(imageId: Int) = silhouette.SecuredAction(errorHandler).async { implicit request =>
    imgurDAO.fetchComments(imageId).map { comments =>
      Ok(Json.obj("success" -> true, "data" -> comments))
    } recover {
      case e: Exception => {
        Logger.error("unable to fetch image comments: " + e.getMessage())
        Ok(Json.obj("success" -> false))
      }
    }
  }
  
  def fetchThread(imageId: Int) = Action.async { implicit request =>
    imgurDAO.fetchThread(imageId).map { thread =>
      Ok(Json.obj("success" -> true, "data" -> thread))
    } recover {
      case e: Exception => {
        Logger.error("unable to fetch image thread: " + e.getMessage())
        Ok(Json.obj("success" -> false))
      }
    }
  }
  
  def image(file: String) = Action {
    Ok.sendFile(env.getFile("imgur/" + file), true).withHeaders("CacheControl" -> "public, max-age=31536000")
  }
  
  def updateCode = silhouette.SecuredAction(WithPermission(AccountPermission.Edit))(errorHandler).async { implicit request =>
    request.body.asFormUrlEncoded match {
      case Some(vals) => {
        val imageId = vals("imageId")(0).toInt
        val codeType = vals("type")(0).toInt
        val codeValue = vals("value")(0).toInt
        val seq = vals("seq")(0).toInt
        
        val code = Code(0, imageId, request.identity.id, CodeType(codeType), CodeValue(codeValue), seq)
        val method = code.value match
        {
          case CodeValue.None => imgurDAO.deleteCode(code)
          case _ => imgurDAO.updateCode(code)
        }
        
        method.map { 
          result => Ok(Json.obj("success" -> result)) 
        }.recover {
          case e: Exception => Ok(Json.obj("success" -> false)) 
        }
      }
      case None => Future.successful(BadRequest)
    }
  }
  
  def updateComment = silhouette.SecuredAction(WithPermission(AccountPermission.Edit))(errorHandler).async { implicit request =>
    request.body.asFormUrlEncoded match {
      case Some(vals) => {
        val imageId = vals("imageId")(0).toInt
        val c = vals("comment")(0)
        
        val comment = Comment(0, imageId, request.identity.id, c)
        val method = (c.length > 0) match {
          case true => imgurDAO.updateComment(comment)
          case false => imgurDAO.deleteComment(comment)
        }
        
        method.map { 
          result => Ok(Json.obj("success" -> result)) 
        }.recover {
          case e: Exception => Ok(Json.obj("success" -> false)) 
        }
      }
      case None => Future.successful(BadRequest)
    }
  }
}