var LoginForm = React.createClass({
	getInitialState: function() {
		return { processing: false, error: false };
	},
	
	handleSubmit: function(e) {
		e.preventDefault();
		e.stopPropagation();
		
		var email = $('#reg-email').val();
		var pass = $('#reg-password').val();

		var data = {
			email: email,
			password: pass,
		};
		
		this.setState({ processing: true, error: false });
		this.props.onsubmit(data, function() {
			this.setState({ processing: false, error: true });
		}.bind(this))
		
		return false;
	},
	
	dismissAlert: function() {
		this.setState({ error: false });
	},
	
	render: function() {
		var processing = this.state.processing === false ? null : 
				(<img src="/assets/images/loading.gif" width="18" height="18" />);
		
		var error = null;
		if (this.state.error) {
			error = (
				<div className="alert alert-danger alert-dismissible" role="alert">
					Login Error
					<button type="button" className="close" onClick={this.dismissAlert}>
						<span aria-hidden="true">&times;</span>
				    </button>
				</div>	
			);
		}
		
		return (
			<form id="login-form" className="standard-form" onSubmit={this.handleSubmit}>
				{error}
				<div className="form-group">
		    		<label for="email">Email Address</label>
		    		<input type="text" name="email" id="reg-email" className="form-control" />
		    	</div>
		    	<div className="form-group">
		   			<label for="password">Password</label>
		   			<input type="password" name="password" id="reg-password" className="form-control" />
		   		</div>
		   		
	   			<button type="submit" className="btn btn-primary" onClick={this.handleSubmit}>Login {processing}</button>
		   	</form>
		);
	}
});