var ImageBrowser = React.createClass({
	_scrollTop: 0,
	_processingScroll: false,
	
	getInitialState: function() {
		return { loading: true, thumbnails: null };
	},
	
	componentDidMount: function() {
		var index = Math.min(this.props.index > 40 ? this.props.index : 40, this.props.data.length);
		var thumbnails = this.buildThumbnails(0, index);
		this.setState({ loading: false, thumbnails: thumbnails });
		
		if (this.props.scrollTop > 0) {
			setTimeout(function() {
				document.documentElement.scrollTop = this.props.scrollTop;
				window.addEventListener('scroll', this.handleScroll);
			}.bind(this), 400);
		} else {
			window.addEventListener('scroll', this.handleScroll);
		}
	},
	
	componentWillUnmount: function() {
		window.removeEventListener('scroll', this.handleScroll);
	},

	handleScroll: function() {
		this._scrollTop = document.documentElement.scrollTop;
		if (!this._processingScroll) {
			window.requestAnimationFrame(function() {
				this.props.onScroll(this._scrollTop);
				
				if (document.documentElement.scrollHeight - this._scrollTop === document.documentElement.clientHeight &&
						this.state.thumbnails.length < this.props.data.length) {
					this.setState({ loading: true });
					this.loadMoreThumbnails();
				}
				
				this._processingScroll = false;
		    }.bind(this));
		}
		
		this._processingScroll = true;	
	}, 
	
    loadMoreThumbnails: function() {
    	setTimeout(function() {
    		var currentLength = this.state.thumbnails.length,
    		    newLength = currentLength + 40 > this.props.data.length ? this.props.data.length : currentLength + 40;
                newThumbnails = this.buildThumbnails(currentLength, newLength);
            
    		this.setState({ loading: false, thumbnails: this.state.thumbnails.concat(newThumbnails) });
    	}.bind(this), 500);
    },
	
	buildThumbnails: function(start, end) {
        var thumbnails = [];
        for (var i = start; i < end; i++) {
        	var meta = this.props.data[i];
        	var img = meta.images[0];
        	var src = "/imgur/" + img.filename.substring(0, img.filename.lastIndexOf('.')) + "_thumb.jpg";
            thumbnails.push(<Thumbnail index={i} src={src} onClick={this.handleClick} />);
        }
        return thumbnails;
	},
	
	handleClick: function(imageIndex) {
		this.props.onImageClick(imageIndex);
	},
	
	render: function() {
		var loading = null;
		if (this.state.loading) {
			loading = (<div className="loading-gif-small"></div>)
		}
		
		return (
			<div className="thumbnailContainer">
				<div className="row image-browser">
	        		{this.state.thumbnails}
	        	</div>
	        	{loading}
			</div>
		);		
	}
});
            
var Thumbnail = React.createClass({
	handleClick: function(e) {
		this.props.onClick(this.props.index);
		e.preventDefault();
	},
	
	render: function() {
		return (
			<div className="col-xs-6 col-md-3">	
				<a href="#" className="thumbnail" onClick={this.handleClick}><img src={this.props.src} /></a>
			</div>
		);
	}
})