package services.account

import com.mohiva.play.silhouette.api.LoginInfo
import com.mohiva.play.silhouette.api.util.PasswordInfo
import com.mohiva.play.silhouette.persistence.daos.DelegableAuthInfoDAO
import play.api.db._
import play.api.Logger
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import anorm._
import anorm.SqlParser._
import anorm.NamedParameter.symbol
import javax.inject.Inject
import scala.concurrent.Future

import utils.Implicits.jsonToString
import models.account.{ AuthProvider, AuthProviderType }

class PasswordAuthInfoService @Inject() (db: Database) extends DelegableAuthInfoDAO[PasswordInfo] {
  val authInfoParser = {
    get[String]("auth_data") map {
      claims => Json.parse(claims)
    }
  }
  
  def find(loginInfo: LoginInfo): Future[Option[PasswordInfo]] = scala.concurrent.Future {
    db.withConnection { implicit c =>
      val providerId = AuthProvider.fromProviderId(loginInfo.providerID)
      var passwordInfo: Option[PasswordInfo] = None
      
      if (!providerId.isEmpty) {
        val res = SQL("SELECT auth_data FROM accounts_auth WHERE provider_id={provider} AND account_key={key}")
              .on('provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .executeQuery()
              
        res.statementWarning match {
          case Some(w) => Logger.error("retrieving PasswordAuthInfo: id=" + loginInfo.providerID + ", key = " + loginInfo.providerKey)
          case None => {
            val claims = res.as(authInfoParser.single)
            passwordInfo = Some(PasswordInfo(
              (claims \ "password_hasher").as[String],
              (claims \ "password_hash").as[String],
              (claims \ "password_salt").asOpt[String]
            ))
          }
        }
      }
      
      passwordInfo
    }
  }
  
  def add(loginInfo: LoginInfo, authInfo: PasswordInfo): Future[PasswordInfo] =  scala.concurrent.Future {
    val providerId = AuthProvider.fromProviderId(loginInfo.providerID)
    
    if (!providerId.isEmpty) {
      db.withConnection { implicit c =>
        val existing = SQL("SELECT count(*) as items FROM accounts_auth WHERE provider_id={provider} AND account_key={key}")
              .on('provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .as(SqlParser.int("items").single)
              
        if (existing == 0) {
          val claims = Json.obj(
              "password_hasher" -> authInfo.hasher,
              "password_hash"		-> authInfo.password,
              "password_salt"   -> authInfo.salt
          )
          
          try {
            val id = SQL("""INSERT INTO accounts_auth (provider_id, provider_type, account_key, auth_data)
                              VALUES ({provider}, {type}, {key}, {claims}) """)
                .on('provider -> providerId.get.id,
                    'type -> AuthProviderType.Credentials.id,
                    'key -> loginInfo.providerKey,
                    'claims -> claims.toString())
                .executeInsert()
                
            if (id.isEmpty) {
              Logger.error("insert into accounts_auth failed");
              throw new Exception("PasswordAuthInfoService.add failed")
            }
          } catch {
            case e: Exception => {
              Logger.error("add authinfo: " + e.getMessage())
            }
          }
        }
      }
    }
    
    authInfo
  }
  
  def update(loginInfo: LoginInfo, authInfo: PasswordInfo): Future[PasswordInfo] = scala.concurrent.Future {
    val providerId = AuthProvider.fromProviderId(loginInfo.providerID)
    
    if (!providerId.isEmpty) {
      db.withConnection { implicit c =>
        val existing = SQL("SELECT count(*) as items FROM accounts_auth WHERE provider_id={provider} AND account_key={key}")
              .on('provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .as(SqlParser.int("items").single)
              
        if (existing == 1) {
          val claims = Json.obj(
              "password_hasher" -> authInfo.hasher,
              "password_hash"		-> authInfo.password,
              "password_salt"   -> authInfo.salt
          )
                    
          val res = SQL("UPDATE accounts_auth SET auth_data={claims} WHERE provider_id={provider} AND account_key={key}")
              .on('claims -> claims.toString(),
                  'provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .executeUpdate()  
        }
      }
    }
    
    authInfo
  }
  
  def save(loginInfo: LoginInfo, authInfo: PasswordInfo): Future[PasswordInfo] = scala.concurrent.Future {
    val providerId = AuthProvider.fromProviderId(loginInfo.providerID)
    
    if (!providerId.isEmpty) {
      db.withConnection { implicit c =>
        val existing = SQL("SELECT count(*) as items FROM accounts_auth WHERE provider_id={provider} AND account_key={key}")
              .on('provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .as(SqlParser.int("items").single)
        
        val claims = Json.obj(
              "password_hasher" -> authInfo.hasher,
              "password_hash"		-> authInfo.password,
              "password_salt"   -> authInfo.salt
        )      
              
        if (existing == 0) {
          val res = SQL("""INSERT INTO accounts_auth (provider_id, provider_type, account_key, auth_data)
                            VALUES ({provider}, {type}, {key}, {claims}) """)
              .on('claims -> claims.toString(),
                  'provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .executeQuery()
        } else {                    
          val res = SQL("UPDATE accounts_auth SET auth_data={claims} WHERE provider_id={provider} AND account_key={key}")
              .on('claims -> claims.toString(),
                  'provider -> providerId.get.id,
                  'key -> loginInfo.providerKey)
              .executeQuery()                  
        }
      }
    }
    
    authInfo
  }
 
  def remove(loginInfo: LoginInfo): Future[Unit] = Future.successful { 
    val providerId = AuthProvider.fromProviderId(loginInfo.providerID)
    
    if (!providerId.isEmpty) {
      db.withConnection { implicit c =>
        SQL("DELETE FROM accounts_auth WHERE provider_id={id} AND account_key={key}")
            .on('id -> providerId.get.id,
                'key -> loginInfo.providerKey)
            .executeUpdate()
      }
    }
  }
}