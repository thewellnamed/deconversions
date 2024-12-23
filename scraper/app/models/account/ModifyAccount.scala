package models.account

import play.api.data.Form
import play.api.data.Forms._

/**
 * The form which handles the sign up process.
 */

object UpdateEmailForm {
  val form = Form(
    mapping(
      "email"     -> email,
      "password"  -> nonEmptyText)(Data.apply)(Data.unapply))
  
  case class Data(
    email: String,
    password: String)
}

object UpdatePasswordForm {
  val form = Form(
    mapping(
      "password"      -> nonEmptyText, 
      "newPassword"   -> nonEmptyText,
      "confirmPassword" -> nonEmptyText
    )(Data.apply)(Data.unapply) verifying(
      "error.confirmPassword", fields => 
        fields match {
          case data => validatePassword(data).isDefined
        }
    )
  )
  
  def validatePassword(data: Data) = {
    (data.newPassword == data.confirmPassword) match {
      case true => Some(data)
      case false => None
    }
  }  
  
  case class Data(
      password: String,
      newPassword: String,
      confirmPassword: String)
}

object UpdateEmailAndPasswordForm {
  val form = Form(
    mapping(
      "email"         -> email,
      "password"      -> nonEmptyText, 
      "newPassword"   -> nonEmptyText,
      "confirmPassword" -> nonEmptyText
    )(Data.apply)(Data.unapply) verifying(
      "error.confirmPassword", fields => 
        fields match {
          case data => validatePassword(data).isDefined
        }
    )
  )
  
  def validatePassword(data: Data) = {
    (data.newPassword == data.confirmPassword) match {
      case true => Some(data)
      case false => None
    }
  }  
  
  case class Data(
      email: String,
      password: String,
      newPassword: String,
      confirmPassword: String)  
}