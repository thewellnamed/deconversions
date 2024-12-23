package controllers

import scala.concurrent.Future
import scala.concurrent.duration._
import java.time._
import java.time.temporal.ChronoUnit
import javax.inject._
import play.api.Configuration
import play.api.mvc._
import play.api.libs.json._
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.Logger
import play.filters.csrf._
import play.filters.csrf.CSRF.Token
import net.ceedubs.ficus.Ficus._
import org.joda.time.Duration
import com.mohiva.play.silhouette.api.{ LoginInfo, Silhouette, SignUpEvent, LoginEvent, LogoutEvent }
import com.mohiva.play.silhouette.api.actions.SecuredErrorHandler
import com.mohiva.play.silhouette.api.repositories.AuthInfoRepository
import com.mohiva.play.silhouette.api.util.{ Clock, Credentials, PasswordHasher, PasswordInfo }
import com.mohiva.play.silhouette.api.exceptions._
import com.mohiva.play.silhouette.impl.exceptions.IdentityNotFoundException
import com.mohiva.play.silhouette.impl.providers.CredentialsProvider

import services.account.{ AccountEnv, AccountService }
import models.account._
import utils.Implicits.jsonWriteAccount

class Accounts @Inject()(
  config: Configuration,
  clock: Clock,
  silhouette: Silhouette[AccountEnv],
  acctService: AccountService,
  authInfoRepository: AuthInfoRepository,
  credentialsProvider: CredentialsProvider,
  passwordHasher: PasswordHasher
) extends Controller {
  
  val errorHandler = new SecuredErrorHandler {
    override def onNotAuthenticated(implicit request: RequestHeader) = {
      Future.successful(Unauthorized)
    }
    override def onNotAuthorized(implicit request: RequestHeader) = {
      Future.successful(Forbidden)
    }
  }  
 
  
  def login = Action.async { implicit request =>
    val csrfToken = CSRF.getToken.get
    val csrf = Json.obj("name" -> csrfToken.name, "value"-> csrfToken.value)
    
    LoginForm.form.bindFromRequest().fold(
      formWithErrors => {
        Future.successful(Ok(Json.obj("success" -> false, "csrf" -> csrf)))
      },
      
      login => {
        credentialsProvider.authenticate(Credentials(login.email, login.password)).flatMap { loginInfo =>
          acctService.retrieve(loginInfo).flatMap {
            case Some(user) => {
              silhouette.env.authenticatorService.create(loginInfo).map {
                case authenticator => {
                  val c = config.underlying
                  val exp = c.as[Int]("silhouette.authenticator.session.expiryDays")

                  authenticator.copy(
                    expirationDateTime = clock.now.plusDays(exp),
                    idleTimeout = c.getAs[FiniteDuration]("silhouette.authenticator.session.idleTimeout")
                  )
                }
              }
            }.flatMap { authenticator =>
              silhouette.env.eventBus.publish(LoginEvent(user, request))
              silhouette.env.authenticatorService.init(authenticator).map { token =>
                val csrfToken = CSRF.getToken.get
                Ok(Json.obj(
                  "success" -> true, 
                  "token"   -> token, 
                  "account" -> user,
                  "csrf"    -> csrf))
              }
            }

            case None => Future.failed(new IdentityNotFoundException("account missing"))
          }
        } recover {
          case e: Exception => Ok(Json.obj("success" -> false, "csrf" -> csrf))
        }
      }
    )
  }
  
  
  def logout = silhouette.SecuredAction(errorHandler).async { implicit request =>
    silhouette.env.eventBus.publish(LogoutEvent(request.identity, request))
    silhouette.env.authenticatorService.discard(request.authenticator, Ok)
    
    val csrfToken = CSRF.getToken.get
    val csrf = Json.obj("name" -> csrfToken.name, "value" -> csrfToken.value)
    Future.successful(Ok(Json.obj("success" -> true, "csrf" -> csrf)))
  }
  
  def modify = silhouette.SecuredAction(errorHandler).async { implicit request =>
    request.body.asFormUrlEncoded match {
      case Some(vals) => {
        val modifyType = vals("type")(0)
        
        modifyType match {
          case "email" => {
            UpdateEmailForm.form.bind(Map(
                "email" -> vals("email")(0),
                "password" -> vals("password")(0)
            )).fold(
              formWithErrors => {
                val errors = formWithErrors.errors.map { e => e.key -> e.messages }.toMap
                Future.successful(Ok(Json.obj("success" -> false, "messages" -> errors)))
              },
              
              form => {
                credentialsProvider.authenticate(Credentials(request.identity.email, form.password)).flatMap { loginInfo =>
                  val authInfo = passwordHasher.hash(form.password)
                  val newLoginInfo = LoginInfo(CredentialsProvider.ID, form.email)
                  
                  for {
                    acct          <- acctService.updateEmail(request.identity, form.email)
                    oldAuth       <- authInfoRepository.remove[PasswordInfo](loginInfo)
                    authInfo      <- authInfoRepository.add(newLoginInfo, authInfo)
                    oldToken      <- silhouette.env.authenticatorService.discard(request.authenticator, Ok)
                    authenticator <- silhouette.env.authenticatorService.create(newLoginInfo)
                    token         <- silhouette.env.authenticatorService.init(authenticator)
                  } yield {
                    Ok(Json.obj("success" -> true, "account" -> acct, "token" -> token, "messages" -> Map("global" -> "msg.modify.email")))
                  }
                } recover {
                  case e: Exception => {
                    Ok(Json.obj("success" -> false, "messages" -> Map("global" -> "error.auth")))
                  }
                }
              }
            )
          }
          
          case "password" => {
            UpdatePasswordForm.form.bind(Map(
                "password" -> vals("password")(0),
                "newPassword" -> vals("newPassword")(0),
                "confirmPassword" -> vals("confirmPassword")(0)
            )).fold(
              formWithErrors => {
                val errors = formWithErrors.errors.map { e => e.key -> e.messages }.toMap
                Future.successful(Ok(Json.obj("success" -> false, "messages" -> errors)))
              },
              
              form => {
                credentialsProvider.authenticate(Credentials(request.identity.email, form.password)).flatMap { loginInfo =>
                  val authInfo = passwordHasher.hash(form.newPassword)
                  for {
                    authInfo      <- authInfoRepository.update[PasswordInfo](loginInfo, authInfo)
                    oldToken      <- silhouette.env.authenticatorService.discard(request.authenticator, Ok)
                    authenticator <- silhouette.env.authenticatorService.create(loginInfo)
                    token         <- silhouette.env.authenticatorService.init(authenticator)
                  } yield {
                    Ok(Json.obj("success" -> true, "account" -> request.identity, "token" -> token, "messages" -> Map("global" -> "msg.modify.password")))
                  }
                } recover {
                  case e: Exception => Ok(Json.obj("success" -> false, "messages" -> Map("global" -> "error.auth")))
                } 
              }
            )
          }
          
          case "combined" => {
            UpdateEmailAndPasswordForm.form.bind(Map(
                "email" -> vals("email")(0),
                "password" -> vals("password")(0),
                "newPassword" -> vals("newPassword")(0),
                "confirmPassword" -> vals("confirmPassword")(0)
            )).fold(
              formWithErrors => {
                val errors = formWithErrors.errors.map { e => e.key -> e.messages }.toMap
                Future.successful(Ok(Json.obj("success" -> false, "messages" -> errors)))
              },
              
              form => {
                credentialsProvider.authenticate(Credentials(request.identity.email, form.password)).flatMap { loginInfo =>
                  val authInfo = passwordHasher.hash(form.newPassword)
                  val newLoginInfo = LoginInfo(CredentialsProvider.ID, form.email)
                  
                  for {
                    acct          <- acctService.updateEmail(request.identity, form.email)
                    oldAuth       <- authInfoRepository.remove[PasswordInfo](loginInfo)
                    authInfo      <- authInfoRepository.add(newLoginInfo, authInfo)
                    oldToken      <- silhouette.env.authenticatorService.discard(request.authenticator, Ok)
                    authenticator <- silhouette.env.authenticatorService.create(newLoginInfo)
                    token         <- silhouette.env.authenticatorService.init(authenticator)
                  } yield {
                    Ok(Json.obj("success" -> true, "account" -> acct, "token" -> token, "messages" -> Map("global" -> "msg.modify.combined")))
                  }
                } recover {
                  case e: Exception => {
                    Ok(Json.obj("success" -> false, "messages" -> Map("global" -> "error.auth")))
                  }
                }
              }
            )
          }
        }
      }
      
      case None => Future.successful(BadRequest)
    } 
  }
}
