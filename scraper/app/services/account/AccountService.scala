package services.account

import com.mohiva.play.silhouette.api.services.IdentityService
import com.mohiva.play.silhouette.api.LoginInfo
import com.mohiva.play.silhouette.api.util.{ PasswordInfo, PasswordHasher }
import play.api.db._
import play.api.Logger
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import anorm._
import anorm.SqlParser._
import anorm.NamedParameter.symbol
import java.time.Instant
import java.time.temporal.ChronoUnit
import javax.inject._
import scala.util.Random
import scala.concurrent.Future

import models.account._

class AccountService @Inject()(db: Database, passwordHasher: PasswordHasher) extends IdentityService[Account] {
  val accountParser = {
    get[Int]("account_id") ~
    get[String]("email") ~
    get[String]("name") ~
    get[Int]("perms") map {
      case id ~ email ~ name ~ perms => 
          Account(id, email, name, perms)
    }
  }
  
  val passwordInfoParser = {
    get[String]("password_hasher") ~
    get[String]("password_hash") ~
    get[Option[String]]("password_salt")  map {
      case hasher ~ hash ~ salt => PasswordInfo(hasher, hash, salt)
    }
  }
  
  // Default settings for new account
  def create(form: RegistrationForm.Data, perms: Int) = {
    Account(0, form.email, form.name, perms)
  }
  
  def fetchAll(): Future[List[Account]] = Future {
    db.withConnection { implicit c =>
      SQL("SELECT account_id, email, name, perms FROM accounts").as(accountParser.*)
    }
  }
  
  def save(account: Account): Future[Account] = Future {
    db.withConnection { implicit c =>
      try {
        val id = SQL("""INSERT INTO accounts (email, name, perms) 
                            VALUES ({email}, {name}, {perms})""")
                        .on('email -> account.email, 'name -> account.name, 'perms -> account.perms)
                        .executeInsert()
        
        id match {
          case Some(v) => Account(v.toInt, account.email, account.name, account.perms)
          case None => throw new Exception("Account save failed")
        }
      } catch {
        case e: Exception => {
          Logger.error("account: save: " + e.getMessage())
          throw e
        }
      }
    }
  }
  
  def retrieve(info: LoginInfo): Future[Option[Account]] = Future {
    db.withConnection { implicit c =>
      val res = SQL("SELECT account_id, email, name, perms FROM accounts WHERE email={email}")
              .on('email -> info.providerKey)
              .executeQuery()
      
      res.statementWarning match {
        case Some(w) => { Logger.error("error retrieving account: " + w.getMessage); None }
        case _ => res.as(accountParser.singleOpt)
      }
    }
  }
  
  def updateEmail(account: Account, email: String): Future[Account] = Future {
    db.withConnection { implicit c =>
      SQL("UPDATE accounts SET email={email} WHERE account_id={id}")
          .on('email -> email, 'id -> account.id)
          .executeUpdate()
          
      Account(account.id, email, account.name, account.perms)
    }
  }
  
  
  def generatePasswordReset() = {
    val newPassword = Random.alphanumeric.take(10).mkString
    PasswordReset(password = newPassword, authInfo = passwordHasher.hash(newPassword))
  }
}