package controllers

import play.api.Environment
import play.api.mvc._
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.Configuration
import play.api.libs.json._
import play.api.Logger
import akka.actor._
import akka.pattern.ask
import akka.routing._
import akka.util.Timeout
import javax.inject._
import java.io.File
import scala.concurrent.{ExecutionContext,TimeoutException}
import scala.concurrent.duration._
import scala.concurrent.Future

import com.mohiva.play.silhouette.api.{ LoginInfo, Silhouette, SignUpEvent, LoginEvent, LogoutEvent }
import com.mohiva.play.silhouette.api.actions.SecuredErrorHandler
import com.mohiva.play.silhouette.api.repositories.AuthInfoRepository
import com.mohiva.play.silhouette.api.util.{ Clock, Credentials, PasswordHasher, PasswordInfo }
import com.mohiva.play.silhouette.api.exceptions._
import com.mohiva.play.silhouette.impl.exceptions.IdentityNotFoundException
import com.mohiva.play.silhouette.impl.providers.CredentialsProvider

import com.sksamuel.scrimage._
import com.sksamuel.scrimage.nio.JpegWriter

import models.account._
import utils.Implicits.jsonWriteAccount
import services.account.{ AccountEnv, AccountService }
import services.deconv._

class Api @Inject()(
  config: Configuration,
  silhouette: Silhouette[AccountEnv],
  acctService: AccountService,
  authInfoRepository: AuthInfoRepository,
  credentialsProvider: CredentialsProvider,
  passwordHasher: PasswordHasher,
  env: Environment,
  @Named("deconv-service") val deconv: ActorRef) extends Controller
{
  val apiKey = config.getString("deconv-study.api.key").get
 
  def fetch(key: String) = Action {
    if (key == apiKey) {
      deconv ! DataRequest("http://www.ex-christian.net/forum/33-pinned-testimonials/")
      Ok("loading data")
    } else {
      Unauthorized
    }
  }  
  
  def updateStats(key: String) = Action {
    if (key == apiKey) {
      deconv ! StatsUpdateRequest
      Ok("updating stats...")
    } else {
      Unauthorized
    }
  }
}