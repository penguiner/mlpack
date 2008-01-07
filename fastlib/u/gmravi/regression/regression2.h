#ifndef REGRESSION2_H
#define REGRESSION2_H
//#include "values.h"
#include "fastlib/fastlib_int.h"

template <typename TKernel> class NaiveRegression2{

 private:

  /** query dataset */
  Matrix qset_;

  /** reference dataset */
  Matrix rset_;

  /** kernel */
  TKernel kernel_;

  /**results stores the density estiamtes of each query point**/
  ArrayList <Matrix> results_;

 public:


  //getter function....

  ArrayList<Matrix>  &get_results(){
   
    //First invert the results.....
   
    for(index_t q=0;q<qset_.n_cols();q++){ //for all query points 
     
      /*This is inversion by SVD **********/
      Vector s;
      Matrix U;
      Matrix VT; 
      Matrix V;
      Matrix S_diagonal;
      Matrix U_transpose;
     
      la::SVDInit(results_[q],&s,&U,&VT); //perform SVD
      //printf("did SVD..\n");
      la::TransposeInit(VT,&V); //Transpose VT
     
      //S_diagonal is a diagonal matrix formed from the reciprocal of the elements of s.
     
      //dimensions of S_diagonal are fromed appropriately. it is (columns in V)X(columns in U)
     
      index_t rows_in_S_diagonal=V.n_cols();
      index_t cols_in_S_diagonal=U.n_cols();
     
      //appropriately initialize s_diagonal
      S_diagonal.Init(rows_in_S_diagonal,cols_in_S_diagonal);
     
      //Fill up the s_diagonal matrix with the reciprocal elements of s
      
      for(index_t i=0;i<rows_in_S_diagonal;i++){
	
	for(index_t j=0;j<cols_in_S_diagonal;j++){
	  
	  if(i==j){
	    
            //The diagonal element
	    S_diagonal.set(i,j,1.0/s[i]);
	  }
	  else{
	    //off diagonal element. hence is equal to 0
	    S_diagonal.set(i,j,0);
	  }
	}
      }
      
      //printf("Did inversion of s vector..\n");
      
      Matrix temp1;
      Matrix temp2;
      la::MulInit (V,S_diagonal,&temp1);
      //printf("did multiplication1..\n");
      
      //Find transpose of U
      
      la::TransposeInit(U,&U_transpose);
      la::MulInit(temp1, U_transpose, &temp2); //At this point the variable temp holds the pseudo-inverse of results_[q]
     
      
      //Copy the contents of temp2 to results_
      results_[q].CopyValues(temp2);
    
    }
     
    return results_;
  }

  //Interesting functions.................
  void Compute (){
  
    // compute unnormalized sum
    for (index_t q = 0; q < qset_.n_cols (); q++){	//for each query point
	
      const double *q_col = qset_.GetColumnPtr (q);
      

      for (index_t row = 0; row < rset_.n_rows () + 1; row++){	//for each row of the matrix
	

	for (index_t col = row; col < rset_.n_rows ()+1; col++){  //for each column of the matrix

	  //temporary varaible
	  double density=0;

	  for(index_t ref=0;ref<rset_.n_cols();ref++){        //for each reference point

	    double weight;
	    
	    if(row>=1&&col>=1){

	      //printf("in row>1 and col>1..\n");

	      weight=rset_.get(row-1,ref)*rset_.get(col-1,ref); 
	      
	      //This means i am considering the row-1th and col-1 th coordinates of the ith point 
	    }
	    
	    else{ 
	      //one of row or columns is 0
	      
	      if(row==0&&col>0){
		
		weight=rset_.get(col-1,ref);
	      }
	      else{
		//row=col=0
		weight=1;
	      }
	    }
	    
	    const double *r_col = rset_.GetColumnPtr (ref); //get the reference point
 	    double dsqd =la::DistanceSqEuclidean (qset_.n_rows (), q_col, r_col);
	    double val = kernel_.EvalUnnormOnSq (dsqd);
	    val *= weight;
	    density+=val;
	  }
	  results_[q].set(row,col,density);
	  
	 
	}
      }
    }

    //reflect this matrix along the diagonal
    for(index_t row=0;row<rset_.n_rows()+1;row++){ //for a row

      for(index_t col=0;col<row;col++){ //for all columns<row => lower triangular matrix
	
	for(index_t q=0;q<qset_.n_cols();q++){ //do this for all query points
	  results_[q].set(row,col,results_[q].get(col,row));
	}
      }
    }
  }

  void Init (Matrix query_dataset, Matrix reference_dataset){

   
    qset_.Copy(query_dataset);
    rset_.Copy(reference_dataset);
    
    kernel_.Init (fx_param_double_req (NULL, "bandwidth"));

    //Initialize results_
    results_.Init (qset_.n_cols ());
    
    for (index_t i = 0; i < qset_.n_cols (); i++){
      
      results_[i].Init(rset_.n_rows()+1,rset_.n_rows()+1); //A square matrix
    }
    
     
  }

  void ComputeMaximumRelativeError(ArrayList<Matrix> fast_wfkde_results){

    FILE *gp;
    gp=fopen("relative_error.txt","w+");

    for(index_t q=0;q<qset_.n_cols();q++){
	
      //max_error is the maximum error u commit for a given query point
      double max_error=0;


	
      for(index_t q=0;q<qset_.n_cols();q++){
	for(index_t row=0;row<rset_.n_rows()+1;row++){
	    
	  for(index_t col=0;col<rset_.n_rows()+1;col++){
	      
	    double error=fabs(fast_wfkde_results[q].get(row,col)-results_[q].get(row,col))/results_[q].get(row,col);
	      
	    if(error>max_error){
	      max_error=error;
	    }
	  }
	}
	fprintf(gp,"The maximum relative error for this query point is %f\n",max_error);
      }
    }
  }
};  //Naive regression2 is done




template <typename TKernel > class Regression2 {

 public:
  
  //forward declaration of Regression2 class
  class Regression2Stat;

  //our tree uses Regression2Stat
  typedef BinarySpaceTree < DHrectBound < 2 >, Matrix, Regression2Stat > Tree;
 
  class Regression2Stat {

  public:
    
    //this is the weight of the kernel desnity estimation which we are going to  use
    
    double weight;
    
    /** lower bound on the densities for the query points owned by this node */
    
    double mass_l;

    /**
     * additional offset for the lower bound on the densities for the query
     * points owned by this node (for leaf nodes only).*/

    double more_l;
    
    /* lower bound offset passed from above. Realise that now it is a 1-D vector*/

    double owed_l;

    /** upper bound on the densities for the query points owned by this node. 
     */

    double mass_u;

    /**
     * additional offset for the upper bound on the densities for the query
     * points owned by this node (for leaf nodes only). 
     */
    double more_u;

    /**
     * upper bound offset passed from above. 
     */

    double owed_u;


    //These are the Init functions of Regression2Stat
    void Init (){

    }

    void Init (const Matrix & dataset, index_t & start, index_t & count){

      Init ();
    }

    void Init (const Matrix & dataset, index_t & start, index_t & count, const Regression2Stat & left_stat, const Regression2Stat & right_stat){
      Init ();
    }

    void MergeChildBounds (Regression2Stat & left_stat, Regression2Stat & right_stat){

      // improve lower and upper bound
      mass_l=max (mass_l, min (left_stat.mass_l, right_stat.mass_l));
      mass_u=min (mass_u, max (left_stat.mass_u, right_stat.mass_u));
    }
    

    Regression2Stat (){
     

    }

    ~Regression2Stat (){
      printf("came to the destructor of regression2stat..\n");

    }

  };//WITH THIS CLASS Regression2STAT COMES TO AN END......

  //Private member of Regression2
 private:
  
  /** query dataset */
  Matrix qset_;

  /**reference dataset */
  Matrix rset_;

  /** query tree */
  Tree *qroot_;

  /** reference tree */
  Tree *rroot_;

  /** list of kernels to evaluate */
  TKernel kernel_;

  /** lower bound on the densities.This will now be a 1-D vector*/
  ArrayList <double> densities_l_;

  /** densities computed .This will now be a 1-D vector*/
  ArrayList <double> densities_e_;

  /** upper bound on the densities.This will now be a 1-D vector */
  ArrayList <double> densities_u_;

  /** accuracy parameter */
  double tau_;

  /** results stores the upper triangular matrix. Just the upper traingular is enough, because  the matrix is symmetric*/
  ArrayList <Matrix> results_;

 
  //Intersting functions...................................



  void scale_data_by_minmax (){
    int num_dims = rset_.n_rows ();
    DHrectBound < 2 > qset_bound;
    DHrectBound < 2 > rset_bound;
    qset_bound.Init (qset_.n_rows ());
    rset_bound.Init (qset_.n_rows ());

    // go through each query/reference point to find out the bounds
    for (index_t r = 0; r < rset_.n_cols (); r++){
      Vector ref_vector;
      rset_.MakeColumnVector (r, &ref_vector);
      rset_bound |= ref_vector;
    }
    for (index_t q = 0; q < qset_.n_cols (); q++){
      Vector query_vector;
      qset_.MakeColumnVector (q, &query_vector);
      qset_bound |= query_vector;
    }

    for (index_t i = 0; i < num_dims; i++){
      DRange qset_range = qset_bound.get (i);
      DRange rset_range = rset_bound.get (i);
      double min_coord = min (qset_range.lo, rset_range.lo);
      double max_coord = max (qset_range.hi, rset_range.hi);
      double width = max_coord - min_coord;
      
      //printf ("Dimension %d range: [%g, %g]\n", i, min_coord, max_coord);
      
      for (index_t j = 0; j < rset_.n_cols (); j++){
	rset_.set (i, j, (rset_.get (i, j) - min_coord) / width);
      }

      if (strcmp (fx_param_str (NULL, "query", NULL),
		  fx_param_str_req (NULL, "data"))){
	for (index_t j = 0; j < qset_.n_cols (); j++){
	  qset_.set (i, j, (qset_.get (i, j) - min_coord) / width);
	}
      }
    }
  }

  void SetUpperBoundOfDensity (Tree * qnode){

    qnode->stat().mass_u= rroot_->stat ().weight;
    
    if (qnode->is_leaf ()){
     
      return;
    }
    
   
    SetUpperBoundOfDensity (qnode->left ());
    SetUpperBoundOfDensity (qnode->right ());
  }

  void PreProcess(Tree *node,index_t row,index_t col){
    
    /** Initialize the statistics */
    
    // initialize lower bound to 0
    

    node->stat ().mass_l = 0;
    node->stat ().owed_l= 0;
    node->stat ().owed_u= 0;


    //Base CAse.....
    
    if (node->is_leaf ()){
      
      //This is a leaf node.......................
	
      node->stat ().more_l=0;
      node->stat ().more_u=0;

      //set weight=ref_{i,row-1}.r_{i,col-1}
      double weight=0;

      if(row>=1&&col>=1){
	  
	//printf("in row>1 and col>1..\n");
	for(index_t i=node->begin();i<node->end();i++){
	    
	  weight+=rset_.get(row-1,i)*rset_.get(col-1,i); //This means i am considering the row-1th and col-1 th corrdinates of the ith point
	}
      }

      else{ //one of row or columns is 0

	if(row==0&&col>0){
	    
	  for(index_t i=node->begin();i<node->end();i++){
	      
	    weight+=rset_.get(col-1,i);
	  }
	}
	else{
	    
	  //printf("row and col both are 0..\n");  
	  weight+=node->end()-node->begin();
	}
      }

      node->stat().weight=weight;
    }
      
    // for non-leaf node, recurse
    else{
      //printf ("This is not a leaf node...\n");
	
      PreProcess (node->left(),row,col);
      PreProcess (node->right(),row,col);
	
      //weight of the parent node is the sum of the weights of the children nodes
	
      node->stat().weight=node->left()->stat().weight + node->right()->stat().weight; 
    }   
  }


  void BestNodePartners (Tree * nd, Tree * nd1, Tree * nd2, Tree ** partner1,Tree ** partner2){
    
    double d1 = nd->bound ().MinDistanceSq (nd1->bound ());
    double d2 = nd->bound ().MinDistanceSq (nd2->bound ());
    
    if (d1 <= d2){
      *partner1 = nd1;
      *partner2 = nd2;
    }
    else{
      *partner1 = nd2;
      *partner2 = nd1;
    }
  }


  void UpdateBounds (Tree * qnode, double dl, double du){
    
    // query self statistics
    Regression2Stat & qstat = qnode->stat ();
    
    //Update mass_l and mass_u
    
    qstat.mass_l += dl;
    qstat.mass_u += du;
    

    // for a leaf node, incorporate the lower and upper bound changes into
    // its additional offset

    if (qnode->is_leaf ()){
      
      qstat.more_l+= dl;
      qstat.more_u+= du;
    }

    // otherwise, incorporate the bound changes into the owed slots of
    // the immediate descendants
    else{
      
      //transmission of the owed values to the children
      qnode->left ()->stat ().owed_l+= dl;
      qnode->left ()->stat ().owed_u+= du;
      
      qnode->right ()->stat ().owed_l+= dl;
      qnode->right ()->stat ().owed_u+= du;
    }
    
   

  }



  /** exhaustive base KDE case */

  void Regression2Base (Tree * qnode, Tree * rnode,int row,int col){
   

    //subtract because now you are doing exhaustive computation
   
    qnode->stat().more_u-=rnode->stat().weight;
   

    // compute unnormalized sum

    for (index_t q = qnode->begin (); q < qnode->end (); q++){

      // get query point
      const double *q_col = qset_.GetColumnPtr(q);

      for (index_t r = rnode->begin (); r < rnode->end (); r++){ 
	  
	// get reference point
	
	const double *r_col = rset_.GetColumnPtr(r);
	
	// pairwise distance and kernel value
       
	double dsqd = la::DistanceSqEuclidean (qset_.n_rows (), q_col, r_col);
	double ker_value = kernel_.EvalUnnormOnSq (dsqd);
       
	if(row>0 && col> 0){
	  
	  densities_l_[q]+= ker_value * rset_.get(row-1,r)*rset_.get(col-1,r);
	  densities_u_[q]+= ker_value * rset_.get(row-1,r)*rset_.get(col-1,r);
	  
	}
	
	else{ //atleast one of row or col is 0
	  
	  if(row==0 && col>0){
	    densities_l_[q]+= ker_value * rset_.get(col-1,r);
	    densities_u_[q]+= ker_value * rset_.get(col-1,r);
	   
	  }
	  
	  else{
	    
	    //both row=col=0;
	    densities_l_[q]+= ker_value;
	    densities_u_[q]+= ker_value; 
	    
	  } 
	}
      }
    }
    
    // get a tighter lower and upper bound for every dimension by looping over each query point
    // in the current query leaf node

    double min_l;
    double  max_u;

    min_l=MAXDOUBLE;
    max_u=-1*MAXDOUBLE;

    for (index_t q = qnode->begin (); q < qnode->end (); q++){

      if (densities_l_[q] < min_l){
	
	min_l= densities_l_[q];
      }
      
      if (densities_u_[q] > max_u){
	
	max_u = densities_u_[q];
      }
    }

    // tighten lower and upper bound
    qnode->stat ().mass_l = min_l + qnode->stat ().more_l;
    qnode->stat ().mass_u = max_u + qnode->stat ().more_u;
    
  }




  int Prunable (Tree * qnode, Tree * rnode, DRange & dsqd_range,DRange & kernel_value_range, double &dl, double &du){

  
   
    // query node stat
    Regression2Stat & stat = qnode->stat ();
   
   
    // try pruning after bound refinement: first compute distance/kernel
    // value bounds
    dsqd_range.lo = qnode->bound ().MinDistanceSq (rnode->bound ());
    dsqd_range.hi = qnode->bound ().MaxDistanceSq (rnode->bound ());
    kernel_value_range = kernel_.RangeUnnormOnSq (dsqd_range);
   
   
    // the new lower bound after incorporating new info for each dimension
   
   
    dl = rnode->stat().weight * kernel_value_range.lo;
    du = -1 *rnode->stat().weight* (1-kernel_value_range.hi);
   
   
    // refine the lower bound using the new lower bound info
   
    double new_mass_l;
   
    new_mass_l = stat.mass_l + dl;
    double allowed_err = tau_ * new_mass_l*((double) (rnode->stat().weight)) /((double) (rroot_->stat().weight));

    // this is error per each query/reference pair for a fixed query
    double m = 0.5 * (kernel_value_range.hi - kernel_value_range.lo);
   
    // this is total maximumn error for each query point

    double error = m * rnode->stat ().weight;

    
    // check pruning condition
    if (error >= allowed_err){
     
      dl=0;
      du=0;
      return 0;
    }
    
    //could prune 
    return 1;
  }



  
  void WFKde(Tree *qnode,Tree *rnode,int row,int col){

    /** temporary variable for storing lower bound change */
    double dl;
    double du;
    dl=0;
    du=0;
    
    
    // temporary variable for holding distance/kernel value bounds
    DRange dsqd_range;
    DRange kernel_value_range;
    
    // query node statistics
    Regression2Stat & stat = qnode->stat ();
    
    // left child and right child of query node statistics

    Regression2Stat *left_stat = NULL;
    Regression2Stat *right_stat = NULL;

    UpdateBounds (qnode, qnode->stat ().owed_l, qnode->stat ().owed_u);
    qnode->stat().owed_l=qnode->stat().owed_u=0;
    
    if (!qnode->is_leaf ()){
      
      left_stat = &(qnode->left ()->stat ());
      right_stat = &(qnode->right ()->stat ());
    
    }
    
    // try finite difference pruning first
    if(Prunable (qnode, rnode, dsqd_range, kernel_value_range, dl, du)==1){
      UpdateBounds (qnode, dl, du);
      return;
    }

  
    //Pruning failed........
    
    if (qnode->is_leaf ()){
      
      if (rnode->is_leaf ()){
	Regression2Base (qnode, rnode,row,col);
	return;
      }
      
      // for non-leaf reference, expand reference node
      else{
	
	Tree *rnode_first = NULL, *rnode_second = NULL;
	BestNodePartners (qnode, rnode->left (), rnode->right (),&rnode_first, &rnode_second);
	WFKde (qnode, rnode_first,row,col);
	WFKde (qnode, rnode_second,row,col);
	return;
      }
    }
    
    // for non-leaf query node
    else{
      // for a leaf reference node, expand query node
      if (rnode->is_leaf ()){
	
	Tree *qnode_first = NULL, *qnode_second = NULL;
       
	BestNodePartners (rnode, qnode->left (), qnode->right (),&qnode_first, &qnode_second);
	WFKde (qnode->left(),rnode,row,col);
	WFKde (qnode->right(),rnode,row,col);
	
      }

      // for non-leaf reference node, expand both query and reference nodes
      else{

	Tree *rnode_first = NULL, *rnode_second = NULL;

	BestNodePartners (qnode->left (), rnode->left (), rnode->right (),&rnode_first, &rnode_second);
	WFKde (qnode->left (), rnode_first,row,col);
	WFKde (qnode->left (), rnode_second,row,col);

	BestNodePartners (qnode->right (), rnode->left (),rnode->right (), &rnode_first, &rnode_second);
	WFKde (qnode->right (), rnode_first,row,col);
	WFKde (qnode->right (), rnode_second,row,col);
       
      }

      stat.MergeChildBounds (*left_stat, *right_stat);
    }
  }

  void PrintDebugTree(Tree *node){


    if(node->is_leaf()){
      return ;
    }

    else{

    
      PrintDebugTree(node->left());
      PrintDebugTree(node->right());
    }
  }
 
  //This function will fill up each element of the matrix for all query points by doing dual tree weighted density estimation

  void FillMatrix(){

    //First create reference and query trees

    int leaflen = fx_param_int (NULL, "leaflen", 10);

    qroot_=tree::MakeKdTreeMidpoint < Tree > (qset_, leaflen,NULL,NULL);
    rroot_=tree::MakeKdTreeMidpoint < Tree > (rset_, leaflen,NULL,NULL);

    //Initialize densities
    densities_l_.Init (qset_.n_cols ());
    densities_u_.Init (qset_.n_cols ());
    densities_e_.Init (qset_.n_cols ());
    
    for(int i=0;i<qset_.n_cols();i++){ 
      
      //set the upper and lower estimates of density of each point to 0
      
      densities_l_[i]=0;
      densities_e_[i]=0;
    }
    
    printf("densities loaded initially..\n");
    //We shall now call kernel density estimate to fill up each element in the matrix. Note that for each iteration the trees need to be flushed 

    for(index_t row=0;row<rset_.n_rows()+1;row++){


      //col is set to row because we are calculating just the upper triangular matrix. This matrix is a (D+1)X(D+1) matrix

      for(index_t col=row;col<rset_.n_rows()+1;col++){ 

	// at the start of every new row,column computation we need to refresh the values in the stat node of the trees

	PreProcess (qroot_,row,col);    
	PreProcess (rroot_,row,col);
	

	//Flush all the densities values

	for(int i=0;i<qset_.n_cols();i++){  //for all points
	  
	  densities_l_[i]=0;
	  densities_e_[i]=0;
	}
	
	for(index_t i=0;i<qset_.n_cols();i++){ //for each query point

	  densities_u_[i]=rroot_->stat().weight;
	}

	//This sets the upper bound on the density at each node
	
	SetUpperBoundOfDensity (qroot_);

	//estimate weighted fast kernel density. 
	
	WFKde(qroot_,rroot_,row,col);
	PostProcess(qroot_,row,col);

	//fill the row,col element of the results witht the estiamted density values
	for(index_t q=0;q<qset_.n_cols();q++){

	  results_[q].set(row,col,densities_e_[q]);
	}

      }
    }

    printf("completed filling matrrix..\n");

    //Reflect the matrix along the daigonal to complete all the entries of the matrix..................
    //Having done this, now reflect the matrix...........
    
    for(index_t row=0;row<rset_.n_rows()+1;row++){ //for a row

      for(index_t col=0;col<row;col++){ //for all columns<row => lower triangular matrix

	for(index_t q=0;q<qset_.n_cols();q++){ //do this for all query points
	  results_[q].set(row,col,results_[q].get(col,row));
	}
      }
    }
  }
 

  void PostProcess(Tree *node,int row, int col){
    
    Regression2Stat &stat = node->stat ();
    UpdateBounds (node, stat.owed_l, stat.owed_u);
    // for leaf query node
    if (node->is_leaf ()){
      
      for (index_t q = node->begin (); q < node->end (); q++){ //for each point
       
	densities_e_[q] =
	  (densities_l_[q] + node->stat ().more_l+
	   densities_u_[q]+ node->stat ().more_u) / 2.0;

      }
    }

    else{

      //It is a non-leaf node
      PostProcess (node->left (),row,col);
      PostProcess (node->right (),row,col);
    }
  }


 

  void InvertAll(){

    //Call SVD first
    printf("Came to invert all");
    
    
    for(index_t q=0;q<qset_.n_cols();q++){ //for all query points 
      
      /*This is SVD stuff used to do pseudo inverse of the matrix**********/
      Vector s;
      Matrix U;
      Matrix VT; 
      Matrix V;
      Matrix S_diagonal;
      Matrix U_transpose;

      la::SVDInit(results_[q],&s,&U,&VT); //perform SVD
      //printf("did SVD..\n");
      la::TransposeInit(VT,&V); //Transpose VT
      //printf("did transpose..\n");

      //S_diagonal is a diagonal matrix formed from the reciprocal of the elements of s.

      //dimensions of S_diagonal are fromed appropriately. it is (columns in V)X(columns in U)

      index_t rows_in_S_diagonal=V.n_cols();
      index_t cols_in_S_diagonal=U.n_cols();

      //appropriately initialize s_diagonal
      S_diagonal.Init(rows_in_S_diagonal,cols_in_S_diagonal);

      //Fill up the s_diagonal matrix with the reciprocal elements of s

      for(index_t i=0;i<rows_in_S_diagonal;i++){

	for(index_t j=0;j<cols_in_S_diagonal;j++){

	  if(i==j){

            //The diagonal element
	    S_diagonal.set(i,j,1.0/s[i]);
	  }
	  else{
	    //off diagonal element. hence is equal to 0
	    S_diagonal.set(i,j,0);
	  }
	}
      }

      //printf("Did inversion of s vector..\n");

      Matrix temp1;
      Matrix temp2;
      la::MulInit (V,S_diagonal,&temp1);
      //printf("did multiplication1..\n");

      //Find transpose of U

      la::TransposeInit(U,&U_transpose);
      la::MulInit(temp1, U_transpose, &temp2); //At this point the variable temp holds the pseudo-inverse of results_[q]
      //printf("did multiplication2..\n");

      //Copy the contents of temp2 to results_
      results_[q].CopyValues(temp2);
     
    }

      
  }


 public:

  ~Regression2(){
    
    printf("came to the destructor of regression2..\n");
  }

  //getters and setters.......


  ArrayList<Matrix>& get_results(){

    return results_;
  }
 

  //Interesting functions.......................... 
  void Compute (double tau){
    
    tau_=tau;
    //This function fills in the Arraylist<matrix> results
    FillMatrix();
    //Do a matrix inversion for the matrix of each query point. 
    InvertAll();
  }
  
  void Init (Matrix &query_dataset, Matrix &reference_dataset){			
    
    
    //This is the Init function of Regression2     
    
    qset_.Alias(query_dataset);
    rset_.Alias(reference_dataset);
     
   
    // scale dataset if the user wants to. 
     
    if (!strcmp (fx_param_str (NULL, "scaling", NULL), "range")){
       
      scale_data_by_minmax ();
    }
     
    // initialize the kernel
     
    kernel_.Init (fx_param_double_req (NULL, "bandwidth"));
     
    printf("init function succesfully executed..\n");

    //Initialize results_ properly
    results_.Init(qset_.n_cols());

    for(int l=0;l<qset_.n_cols();l++){

      results_[l].Init(rset_.n_rows()+1,rset_.n_rows()+1); //dimensions of each matrix are (D+1)X(D+1), where D is the dimensionality of the reference dataset
    }
       
  }
  
}; //Class Regression2 comes to an end..............



#endif



