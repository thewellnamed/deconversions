package services.account

import com.mohiva.play.silhouette.api.StorableAuthenticator
import com.mohiva.play.silhouette.api.repositories.AuthenticatorRepository
import com.mohiva.play.silhouette.impl.authenticators.JWTAuthenticator
import com.mohiva.play.silhouette.impl.providers.CredentialsProvider
import com.mohiva.play.silhouette.api.LoginInfo
import play.api.db._
import play.api.Logger
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import anorm._
import anorm.SqlParser._
import anorm.NamedParameter.symbol
import anorm.JodaParameterMetaData._
import scala.concurrent.Future
import scala.concurrent.duration._
import org.joda.time._
import javax.inject._

import models.account.AuthProvider;

class SessionService @Inject()(db: Database) extends AuthenticatorRepository[JWTAuthenticator] {
  val sessionParser = {
    get[String]("session_token") ~
    get[Int]("provider_id") ~
    get[String]("account_key") ~
    get[DateTime]("last_updated") ~
    get[DateTime]("expires") ~
    get[Option[Long]]("idle_timeout") ~
    get[Option[String]]("claims") map {
      case token ~ providerId ~ key ~ lastUpdate ~ expires ~ idleOpt ~ claimOpt => {
        val idle = idleOpt match {
          case Some(i) => Some(FiniteDuration(i, "seconds"))
          case None => None
        }
        val claims = claimOpt match {
          case Some(c) => Some(Json.parse(c).as[JsObject])
          case None => None
        }
        
        val provider = AuthProvider(providerId).toString()
        
        JWTAuthenticator(token, LoginInfo(provider, key), lastUpdate, expires, idle, claims)
      }
    }
  }
  
  override def find(id: String): Future[Option[JWTAuthenticator]] = Future {
    db.withConnection { implicit c =>
      val res = SQL("""SELECT session_token, provider_id, account_key, last_updated, expires, idle_timeout, claims
                       FROM sessions WHERE session_hash=md5({id})::uuid""")
              .on('id -> id)
              .executeQuery()
              
      res.statementWarning match {
        case Some(w) => { Logger.error("error retrieving authenticator: " + w.getMessage); None }
        case _ => res.as(sessionParser.singleOpt)
      }
    }
  }

  override def add(authenticator: JWTAuthenticator): Future[JWTAuthenticator] = Future {
    db.withConnection { implicit c =>
      val customClaims = authenticator.customClaims match {
        case Some(claims) => Some(claims.toString())
        case None => None
      }
      val idleTimeout = authenticator.idleTimeout match {
        case Some(idle) => Some(idle.toSeconds)
        case None => None
      }
        
      SQL("""INSERT INTO sessions (session_hash, session_token, provider_id, account_key, last_updated, expires, idle_timeout, claims)
                       VALUES (md5({token})::uuid, {token}, {provider}, {key}, {lastUpdated}, {expires}, {idleTimeout}, {claims})""")
              .on('token         -> authenticator.id,
                  'provider      -> AuthProvider.fromProviderId(authenticator.loginInfo.providerID).getOrElse(AuthProvider.Credentials).id,
                  'key           -> authenticator.loginInfo.providerKey,
                  'lastUpdated   -> authenticator.lastUsedDateTime,
                  'expires       -> authenticator.expirationDateTime,
                  'idleTimeout   -> idleTimeout,
                  'claims        -> customClaims)
              .executeInsert(SqlParser.scalar[java.util.UUID].single)
                        
      authenticator
    } 
  }

  override def update(authenticator: JWTAuthenticator): Future[JWTAuthenticator] = scala.concurrent.Future {
    db.withConnection { implicit c =>
      val customClaims = authenticator.customClaims match {
        case Some(claims) => Some(claims.toString())
        case None => None
      }
      val idleTimeout = authenticator.idleTimeout match {
        case Some(idle) => Some(idle.toSeconds)
        case None => None
      }
      
     val res = SQL("""UPDATE sessions SET
                         provider_id = {provider},
                         account_key = {key}, 
                         last_updated = {lastUpdated}, 
                         expires = {expires}, 
                         idle_timeout = {idleTimeout}, 
                         claims = {claims}
                      WHERE session_hash = md5({token})::uuid""")
              .on('token         -> authenticator.id,
                  'provider      -> AuthProvider.fromProviderId(authenticator.loginInfo.providerID).getOrElse(AuthProvider.Credentials).id,
                  'key           -> authenticator.loginInfo.providerKey,
                  'lastUpdated   -> authenticator.lastUsedDateTime,
                  'expires       -> authenticator.expirationDateTime,
                  'idleTimeout   -> idleTimeout,
                  'claims        -> customClaims)
              .executeUpdate()
               
      authenticator
    }
  }

  override def remove(id: String): Future[Unit] = scala.concurrent.Future {
    db.withConnection { implicit c =>
      val res = SQL("DELETE FROM sessions WHERE session_hash = md5({token})::uuid")
         .on('token -> id)
         .executeUpdate()
    }
  }
}