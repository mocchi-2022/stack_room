/****************************************************************************************************************************
**
**         MIST ( Media Integration Standard Toolkit )
**
**         matrix template class implementation using expression template technique.
**
**
**           All matrix operations are described in row order.
**           All matrix elements are allocated as one dimensional array on the memory space.
**
**             ex) The order of 3x3 matrix are
**
**                   1  4  7
**                   2  5  8
**                   3  6  9
**
**
**         Please use matrix( row, col ) accesses operation. 
**
**
**
**         We developed these programs since 2003/09/05.
**
**             $LastChangedDate::                      $
**             $LastChangedRevision$
**             $LastChangedBy$
**             $HeadURL$
**
**              Copyright ***********************.
**                    All Rights Reserved.
**
****************************************************************************************************************************/

// 
// Copyright (c) 2003-2010, MIST Project, Nagoya University
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
// 
// 3. Neither the name of the Nagoya University nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 



/// @file mist/matrix.h
//!
//! @brief 一般行列を扱うためのクラス
//!
#ifndef __INCLUDE_MIST_MATRIX__
#define __INCLUDE_MIST_MATRIX__

#include <complex>

#ifndef __INCLUDE_MIST_H__
#include "mist.h"
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

#include <cmath>


// mist名前空間の始まり
_MIST_BEGIN



namespace __numeric__
{
	template < class T >
	struct is_complex
	{
		_MIST_CONST( bool, value, false );
	};

#if defined(__MIST_MSVC__) && __MIST_MSVC__ < 7

	#define IS_COMPLEX( type ) \
		template < >\
	struct is_complex< std::complex< type > >\
		{\
		enum{ value = true };\
		};\

		// 各型に対する特殊化
		IS_COMPLEX(unsigned char)
		IS_COMPLEX(unsigned short)
		IS_COMPLEX(unsigned int)
		IS_COMPLEX(unsigned long)
		IS_COMPLEX(signed char)
		IS_COMPLEX(signed short)
		IS_COMPLEX(signed int)
		IS_COMPLEX(signed long)
		IS_COMPLEX(bool)
		IS_COMPLEX(char)
		IS_COMPLEX(float)
		IS_COMPLEX(double)
		IS_COMPLEX(long double)

	#undef IS_COLOR

#else

	template < class T >
	struct is_complex< std::complex< T > >
	{
		_MIST_CONST( bool, value, true );
	};

#endif

	template< bool b >
	struct value_compare
	{
		template < class T > static bool le( const T &v1, const T &v2 ){ return( v1 <= v2 ); }
		template < class T > static bool ge( const T &v1, const T &v2 ){ return( v1 >= v2 ); }
		template < class T > static bool lt( const T &v1, const T &v2 ){ return( v1 <  v2 ); }
		template < class T > static bool gt( const T &v1, const T &v2 ){ return( v1 >  v2 ); }
		template < class T > static bool eq( const T &v1, const T &v2 ){ return( v1 == v2 ); }
		template < class T > static bool eq( const T &v1, const T &v2, const double delta ){ return( std::abs( v1 - v2 ) < delta ); }
		template < class T > static bool is_zero( const T &v ){ return( v == T( ) ); }
	};

	template< >
	struct value_compare< true >
	{
		template < class T >
		static bool lt( const T &v1, const T &v2 )
		{
			if( v1.real( ) < v2.real( ) )
			{
				return( true );
			}
			else if( v1.real( ) > v2.real( ) )
			{
				return( false );
			}
			else if( v1.imag( ) < v2.imag( ) )
			{
				return( true );
			}
			else
			{
				return( false );
			}
		}
		template < class T > static bool ge( const T &v1, const T &v2 ){ return( !lt( v1, v2 ) ); }
		template < class T > static bool le( const T &v1, const T &v2 ){ return( !lt( v2, v1 ) ); }
		template < class T > static bool gt( const T &v1, const T &v2 ){ return( lt( v2, v1 ) ); }
		template < class T > static bool eq( const T &v1, const T &v2 )
		{
			return( v1.real( ) == v2.real( ) && v1.imag( ) == v2.imag( ) );
		}
		template < class T > static bool eq( const T &v1, const T &v2, const double delta )
		{
			return( std::abs( v1.real( ) - v2.real( ) ) < delta && std::abs( v1.imag( ) - v2.imag( ) ) < delta );
		}
		template < class T > static bool is_zero( const T &v ){ return( v.real( ) == 0 && v.imag( ) == 0 ); }
	};

	// 共役複素数を返す関数
	template< class T > std::complex< T > conjugate( const std::complex< T > &c ) { return std::conj( c ); }
	template< class T > const T &conjugate( const T &c ) { return c; }
}


/// @brief 行と列の要素数が可変の行列
//! 
//! - _CHECK_MATRIX_OPERATION_  マクロが1の場合は，行列演算の左辺と右辺で行列のサイズが適切であるかをチェックする
//! - _CHECK_ACCESS_VIOLATION_  マクロが1の場合は，行列の要素へアクセスする際に，範囲外かどうかを判定する
//! 
//! @param T         … 行列内に格納するデータ型
//! @param Allocator … MISTコンテナが利用するアロケータ型．省略した場合は，STLのデフォルトアロケータを使用する
//! 
template < class T, class Allocator = ::std::allocator< T > >
class matrix : public array< T, Allocator >
{
public:
	typedef Allocator allocator_type;								///< @brief MISTコンテナが利用するアロケータ型
	typedef typename Allocator::reference reference;				///< @brief MISTのコンテナ内に格納するデータ型の参照．mist::matrix< data > の場合，data & となる
	typedef typename Allocator::const_reference const_reference;	///< @brief MISTのコンテナ内に格納するデータ型の const 参照．mist::matrix< data > の場合，const data & となる
	typedef typename Allocator::value_type value_type;				///< @brief MISTのコンテナ内に格納するデータ型．mist::matrix< data > の data と同じ
	typedef typename Allocator::size_type size_type;				///< @brief 符号なしの整数を表す型．コンテナ内の要素数や，各要素を指定するときなどに利用し，内部的には size_t 型と同じ
	typedef typename Allocator::difference_type difference_type;	///< @brief 符号付きの整数を表す型．コンテナ内の要素数や，各要素を指定するときなどに利用し，内部的には ptrdiff_t 型と同じ
	typedef typename Allocator::pointer pointer;					///< @brief MISTのコンテナ内に格納するデータ型のポインター型．mist::matrix< data > の場合，data * となる
	typedef typename Allocator::const_pointer const_pointer;		///< @brief MISTのコンテナ内に格納するデータ型の const ポインター型．mist::matrix< data > の場合，const data * となる

	/// @brief MISTのコンテナ内を操作する，STLで言う順方向のランダムアクセスイテレータ
	typedef mist_iterator1< T, ptrdiff_t, pointer, reference > iterator;

	/// @brief MISTのコンテナ内を操作する，順方向のランダムアクセスイテレータのコンスト版
	typedef mist_iterator1< T, ptrdiff_t, const_pointer, const_reference > const_iterator;

	/// @brief MISTのコンテナ内を操作する，逆方向のランダムアクセスイテレータ
	typedef mist_reverse_iterator< mist_iterator1< T, ptrdiff_t, pointer, reference > > reverse_iterator;

	/// @brief MISTのコンテナ内を操作する，逆方向のランダムアクセスイテレータのコンスト版
	typedef mist_reverse_iterator< mist_iterator1< T, ptrdiff_t, const_pointer, const_reference > > const_reverse_iterator;

	/// @brief データ型の変換を行う
	template < class TT, class AAllocator = std::allocator< TT > > 
	struct rebind
	{
		typedef matrix< TT, AAllocator > other;
	};

private:
	typedef array< T, Allocator > base;
	size_type size2_;
	size_type size1_;

private:
	// rows や cols との混乱を避けるため，matrix では使えないようにする
	size_type width( ) const;	///< @brief X軸方向のコンテナに格納されているデータ数を返す
	size_type height( ) const;	///< @brief Y軸方向のコンテナに格納されているデータ数を返す（常に1を返す）
	size_type depth( ) const;	///< @brief Z軸方向のコンテナに格納されているデータ数を返す（常に1を返す）

public:
	/// @brief 行列のサイズを num 行 1 列に変更する
	//! 
	//! 元の行列と異なる場合は，自動的にサイズを調整する．
	//! 全ての要素の値はデフォルトコンストラクタで初期化される．
	//! また，組み込み型（int や double など）の場合は，全ての要素を0で初期化する．
	//! 
	//! @param[in] num … リサイズ後の行列の行数
	//! 
	//! @retval true  … 正常にリサイズが終了
	//! @retval false … リサイズ後のメモリを確保できなかった場合
	//! 
	bool resize( size_type num )
	{
		if( base::resize( num ) )
		{
			size1_ = num;
			size2_ = 1;
			return( true );
		}
		else
		{
			size1_ = size2_ = 0;
			return( false );
		}
	}


	/// @brief 行列のサイズを num1 行 num2 列に変更する
	//! 
	//! 元の行列サイズと異なる場合は，自動的にサイズを調整する．
	//! 全ての要素の値はデフォルトコンストラクタで初期化される．
	//! また，組み込み型（int や double など）の場合は，全ての要素を0で初期化する．
	//! 
	//! @param[in] nrows … リサイズ後の行列の行数
	//! @param[in] ncols … リサイズ後の行列の列数
	//! 
	//! @retval true  … 正常にリサイズが終了
	//! @retval false … リサイズ後のメモリを確保できなかった場合
	//! 
	bool resize( size_type nrows, size_type ncols )
	{
		if( base::resize( nrows * ncols ) )
		{
			size1_ = nrows;
			size2_ = ncols;
			return( true );
		}
		else
		{
			size1_ = size2_ = 0;
			return( false );
		}
	}

	/// @brief 行列のサイズを num1 行 num2 列に変更する
	//! 
	//! 行列サイズを num1 行 num2 列に変更し，全ての要素を値 val で初期化する．
	//! 
	//! @param[in] nrows … リサイズ後の行列の行数
	//! @param[in] ncols … リサイズ後の行列の列数
	//! @param[in] val   … リサイズ後に各要素を初期化する値
	//! 
	//! @retval true  … 正常にリサイズが終了
	//! @retval false … リサイズ後のメモリを確保できなかった場合
	//! 
	bool resize( size_type nrows, size_type ncols, const T &val )
	{
		if( base::resize( nrows * ncols, val ) )
		{
			size1_ = nrows;
			size2_ = ncols;
			return( true );
		}
		else
		{
			size1_ = size2_ = 0;
			return( false );
		}
	}

	/// @brief コンテナ内の要素をトリミングする
	//! 
	//! @param[out] out   … トリミング結果の行列を格納するオブジェクト（行列のサイズは自動で変更されます）
	//! @param[in]  row   … トリミングの行方向の開始位置（ゼロから始まるインデックス）
	//! @param[in]  col   … トリミングの列方向の開始位置（ゼロから始まるインデックス）
	//! @param[in]  nrows … トリミング後の行数（-1の場合は，最後までをコピーする）
	//! @param[in]  ncols … トリミング後の列数（-1の場合は，最後までをコピーする）
	//! 
	//! @retval true  … トリミングに成功した場合（元とサイズが変わらない場合も含む）
	//! @retval false … 不正なトリミングを行おうとした場合
	//! 
	bool trim( matrix &out, size_type row, size_type col, difference_type nrows = -1, difference_type ncols = -1 ) const
	{
		difference_type nrows_ = rows( );
		difference_type ncols_ = cols( );
		if( nrows_ <= static_cast< difference_type >( row ) || nrows_ < static_cast< difference_type >( row + nrows ) )
		{
			return( false );
		}
		else if( ncols_ <= static_cast< difference_type >( col ) || ncols_ < static_cast< difference_type >( col + ncols ) )
		{
			return( false );
		}
		else if( nrows_ == nrows && ncols_ == ncols )
		{
			return( true );
		}

		if( nrows < 0 )
		{
			nrows = nrows_ - row;
		}
		if( ncols < 0 )
		{
			ncols = ncols_ - col;
		}

		if( out.resize( nrows, ncols ) )
		{
			const_pointer pi = paccess( row, col );
			pointer       po = out.paccess( 0, 0 );
			for( difference_type c = 0 ; c < ncols ; c++ )
			{
				po = base::allocator_.copy_objects( pi, nrows, po );
				pi += this->rows( );
			}

			return( true );
		}
		else
		{
			return( false );
		}
	}

	/// @brief コンテナ内の要素をトリミングする
	//! 
	//! @param[in] row   … トリミングの行方向の開始位置（ゼロから始まるインデックス）
	//! @param[in] col   … トリミングの列方向の開始位置（ゼロから始まるインデックス）
	//! @param[in] nrows … トリミング後の行数（-1の場合は，最後までをコピーする）
	//! @param[in] ncols … トリミング後の列数（-1の場合は，最後までをコピーする）
	//! 
	//! @retval true  … トリミングに成功した場合（元とサイズが変わらない場合も含む）
	//! @retval false … 不正なトリミングを行おうとした場合
	//! 
	bool trim( size_type row, size_type col, difference_type nrows = -1, difference_type ncols = -1 )
	{
		if( base::is_memory_shared( ) )
		{
			// 外部メモリを利用している場合
			matrix o( *this );
			return( o.trim( *this, row, col, nrows, ncols ) );
		}
		else
		{
			matrix o;

			if( this->trim( o, row, col, nrows, ncols ) )
			{
				swap( o );

				return( true );
			}
			else
			{
				return( false );
			}
		}
	}

	/// @brief 行列内の全ての内容を入れ替える．
	//! 
	//! 入れ替え元の行列 m の中身と全て入れ替える
	//! 
	//! @param[in] m … 内容を入れ替える対象
	//! 
	//! @retval true  … データのスワップに成功
	//! @retval false … データのスワップに失敗
	//! 
	bool swap( matrix &m )
	{
		if( base::swap( m ) )
		{
			size_type _dmy_size1 = size1_;
			size_type _dmy_size2 = size2_;
			size1_ = m.size1_;
			size2_ = m.size2_;
			m.size1_ = _dmy_size1;
			m.size2_ = _dmy_size2;
			return( true );
		}
		else
		{
			return( false );
		}
	}

	/// @brief 行列の要素を空にする
	//! 
	//! 行列に格納されているデータを全て削除し，行列を空（0行0列）にする
	//! 
	void clear( )
	{
		base::clear( );
		size1_ = size2_ = 0;
	}


	size_type size1( ) const { return( size1_ ); }	///< @brief 行数を返す
	size_type size2( ) const { return( size2_ ); }	///< @brief 列数を返す
	size_type rows( ) const { return( size1_ ); }	///< @brief 行数を返す
	size_type cols( ) const { return( size2_ ); }	///< @brief 列数を返す



/************************************************************************************************************
**
**      行に対する順方向・逆方向の反復子
**
************************************************************************************************************/

	/// @brief 行方向を操作する順方向ランダムアクセスイテレータを返す
	//!
	//! @code 行方向を操作する順方向ランダムアクセスイテレータの使用例
	//! std::cout << "順方向ランダムアクセスイテレータ" << std::endl;
	//! mist::array< int >::iterator ite1 = a.row_begin( 1 );
	//! for( ; ite1 != a.row_end( 1 ) ; ite1++ )
	//! {
	//! 	std::cout << *ite1 << " ";
	//! }
	//! std::cout << std::endl << std::endl;
	//! @endcode
	//!
	iterator row_begin( size_type r ){ return( iterator( paccess( r, 0 ), rows( ) ) ); }

	/// @brief 行方向を操作するコンスト型の順方向ランダムアクセスイテレータを返す
	const_iterator row_begin( size_type r ) const { return( const_iterator( paccess( r, 0 ), rows( ) ) ); }

	/// @brief 行方向を操作する順方向ランダムアクセスイテレータを返す
	iterator row_end( size_type r ){ return( iterator( paccess( r, cols( ) ), rows( ) ) ); }

	/// @brief 行方向を操作するコンスト型の順方向ランダムアクセスイテレータを返す
	const_iterator row_end( size_type r ) const { return( const_iterator( paccess( r, cols( ) ), rows( ) ) ); }


	/// @brief 行方向を操作する逆方向ランダムアクセスイテレータを返す
	//!
	//! @code 行方向を操作する逆方向ランダムアクセスイテレータの使用例
	//! std::cout << "逆方向ランダムアクセスイテレータ" << std::endl;
	//! mist::array< int >::iterator ite1 = a.row_rbegin( 1 );
	//! for( ; ite1 != a.row_rend( 1 ) ; ite1++ )
	//! {
	//! 	std::cout << *ite1 << " ";
	//! }
	//! std::cout << std::endl << std::endl;
	//! @endcode
	//!
	reverse_iterator row_rbegin( size_type r ){ return( reverse_iterator( row_end( r )  ) ); }

	/// @brief 行方向を操作するコンスト型の逆方向ランダムアクセスイテレータを返す
	const_reverse_iterator row_rbegin( size_type r ) const { return( const_reverse_iterator( row_end( r ) ) ); }

	/// @brief 行方向を操作する逆方向ランダムアクセスイテレータを返す
	reverse_iterator row_rend( size_type r ){ return( reverse_iterator( row_begin( r ) ) ); }

	/// @brief 行方向を操作するコンスト型の逆方向ランダムアクセスイテレータを返す
	const_reverse_iterator row_rend( size_type r ) const { return( const_reverse_iterator( row_begin( r ) ) ); }


/************************************************************************************************************
**
**      列に対する順方向・逆方向の反復子
**
************************************************************************************************************/

	/// @brief 列方向を操作する順方向ランダムアクセスイテレータを返す
	//!
	//! @code 列方向を操作する順方向ランダムアクセスイテレータの使用例
	//! std::cout << "順方向ランダムアクセスイテレータ" << std::endl;
	//! mist::array< int >::iterator ite1 = a.row_begin( 1 );
	//! for( ; ite1 != a.row_end( 1 ) ; ite1++ )
	//! {
	//! 	std::cout << *ite1 << " ";
	//! }
	//! std::cout << std::endl << std::endl;
	//! @endcode
	//!
	iterator col_begin( size_type c ){ return( iterator( paccess( 0, c ), 1 ) ); }

	/// @brief 列方向を操作するコンスト型の順方向ランダムアクセスイテレータを返す
	const_iterator col_begin( size_type c ) const { return( const_iterator( paccess( 0, c ), 1 ) ); }

	/// @brief 列方向を操作する順方向ランダムアクセスイテレータを返す
	iterator col_end( size_type c ){ return( iterator( paccess( rows( ), c ), 1 ) ); }

	/// @brief 列方向を操作するコンスト型の順方向ランダムアクセスイテレータを返す
	const_iterator col_end( size_type c ) const { return( const_iterator( paccess( rows( ), c ), 1 ) ); }


	/// @brief 列方向を操作する逆方向ランダムアクセスイテレータを返す
	//!
	//! @code 列方向を操作する逆方向ランダムアクセスイテレータの使用例
	//! std::cout << "逆方向ランダムアクセスイテレータ" << std::endl;
	//! mist::array< int >::iterator ite1 = a.row_rbegin( 1 );
	//! for( ; ite1 != a.row_rend( 1 ) ; ite1++ )
	//! {
	//! 	std::cout << *ite1 << " ";
	//! }
	//! std::cout << std::endl << std::endl;
	//! @endcode
	//!
	reverse_iterator col_rbegin( size_type c ){ return( reverse_iterator( col_end( c ) ) ); }

	/// @brief 列方向を操作するコンスト型の逆方向ランダムアクセスイテレータを返す
	const_reverse_iterator col_rbegin( size_type c ) const { return( const_reverse_iterator(  col_end( c ) ) ); }

	/// @brief 列方向を操作する逆方向ランダムアクセスイテレータを返す
	reverse_iterator col_rend( size_type c ){ return( reverse_iterator( col_begin( c ) ) ); }

	/// @brief 列方向を操作するコンスト型の逆方向ランダムアクセスイテレータを返す
	const_reverse_iterator col_rend( size_type c ) const { return( const_reverse_iterator( col_begin( c ) ) ); }





public:
	/**  @brief 3×3 の対角行列を計算する
	 *  
	 *  3つの入力成分の大きいものから順番に並べ替えて，対角行列を作成する
	 *  
	 *  \f[
	 *      \left(
	 *           \begin{array}{ccc}
	 *             \sigma_1 &    0     &    0     \\
	 *                0     & \sigma_2 &    0     \\
	 *                0     &    0     & \sigma_3
	 *           \end{array}
	 *      \right)
	 *  \f]
	 *  ただし，\f$ \sigma_1 \ge \sigma_2 \ge \sigma_3 \f$
	 *  
	 *  @param[in] s1 … 成分1
	 *  @param[in] s2 … 成分2
	 *  @param[in] s3 … 成分3
	 * 
	 *  @return 対角行列
	 */
	static const matrix diag( const value_type &s1, const value_type &s2, const value_type &s3 )
	{
		typedef __numeric__::value_compare< __numeric__::is_complex< value_type >::value > value_compare;
		matrix d( 3, 3 );

		if( value_compare::lt( s1, s2 ) )
		{
			// s1 < s2
			if( value_compare::lt( s1, s3 ) )
			{
				if( value_compare::lt( s2, s3 ) )
				{
					d( 0, 0 ) = s3;
					d( 1, 1 ) = s2;
					d( 2, 2 ) = s1;
				}
				else
				{
					d( 0, 0 ) = s2;
					d( 1, 1 ) = s3;
					d( 2, 2 ) = s1;
				}
			}
			else
			{
				d( 0, 0 ) = s2;
				d( 1, 1 ) = s1;
				d( 2, 2 ) = s3;
			}
		}
		else
		{
			// s2 < s1
			if( value_compare::lt( s1, s3 ) )
			{
				d( 0, 0 ) = s3;
				d( 1, 1 ) = s1;
				d( 2, 2 ) = s2;
			}
			else
			{
				if( value_compare::lt( s2, s3 ) )
				{
					d( 0, 0 ) = s1;
					d( 1, 1 ) = s3;
					d( 2, 2 ) = s2;
				}
				else
				{
					d( 0, 0 ) = s1;
					d( 1, 1 ) = s2;
					d( 2, 2 ) = s3;
				}
			}
		}
		return( d );
	}



	/// @brief 入力データを用いて4×4の行列を返す
	static const matrix _44(
								const value_type &a00, const value_type &a01, const value_type &a02, const value_type &a03,
								const value_type &a10, const value_type &a11, const value_type &a12, const value_type &a13,
								const value_type &a20, const value_type &a21, const value_type &a22, const value_type &a23,
								const value_type &a30, const value_type &a31, const value_type &a32, const value_type &a33
							)
	{
		matrix o( 4, 4 );

		o( 0, 0 ) = a00; o( 0, 1 ) = a01; o( 0, 2 ) = a02; o( 0, 3 ) = a03;
		o( 1, 0 ) = a10; o( 1, 1 ) = a11; o( 1, 2 ) = a12; o( 1, 3 ) = a13;
		o( 2, 0 ) = a20; o( 2, 1 ) = a21; o( 2, 2 ) = a22; o( 2, 3 ) = a23;
		o( 3, 0 ) = a30; o( 3, 1 ) = a31; o( 3, 2 ) = a32; o( 3, 3 ) = a33;

		return( o );
	}

	/// @brief 入力データを用いて3×4の行列を返す
	static const matrix _34(
								const value_type &a00, const value_type &a01, const value_type &a02, const value_type &a03,
								const value_type &a10, const value_type &a11, const value_type &a12, const value_type &a13,
								const value_type &a20, const value_type &a21, const value_type &a22, const value_type &a23
							)
	{
		matrix o( 3, 4 );

		o( 0, 0 ) = a00; o( 0, 1 ) = a01; o( 0, 2 ) = a02; o( 0, 3 ) = a03;
		o( 1, 0 ) = a10; o( 1, 1 ) = a11; o( 1, 2 ) = a12; o( 1, 3 ) = a13;
		o( 2, 0 ) = a20; o( 2, 1 ) = a21; o( 2, 2 ) = a22; o( 2, 3 ) = a23;

		return( o );
	}

	/// @brief 入力データを用いて4×3の行列を返す
	static const matrix _43(
								const value_type &a00, const value_type &a01, const value_type &a02,
								const value_type &a10, const value_type &a11, const value_type &a12,
								const value_type &a20, const value_type &a21, const value_type &a22,
								const value_type &a30, const value_type &a31, const value_type &a32
							)
	{
		matrix o( 4, 3 );

		o( 0, 0 ) = a00; o( 0, 1 ) = a01; o( 0, 2 ) = a02;
		o( 1, 0 ) = a10; o( 1, 1 ) = a11; o( 1, 2 ) = a12;
		o( 2, 0 ) = a20; o( 2, 1 ) = a21; o( 2, 2 ) = a22;
		o( 3, 0 ) = a30; o( 3, 1 ) = a31; o( 3, 2 ) = a32;

		return( o );
	}

	/// @brief 入力データを用いて4×1の行列を返す
	static const matrix _41( const value_type &a0, const value_type &a1, const value_type &a2, const value_type &a3 )
	{
		matrix o( 4, 1 );

		o[ 0 ] = a0;
		o[ 1 ] = a1;
		o[ 2 ] = a2;
		o[ 3 ] = a3;

		return( o );
	}

	/// @brief 入力データを用いて3×3の行列を返す
	static const matrix _33(
								const value_type &a00, const value_type &a01, const value_type &a02,
								const value_type &a10, const value_type &a11, const value_type &a12,
								const value_type &a20, const value_type &a21, const value_type &a22
							)
	{
		matrix o( 3, 3 );

		o( 0, 0 ) = a00; o( 0, 1 ) = a01; o( 0, 2 ) = a02;
		o( 1, 0 ) = a10; o( 1, 1 ) = a11; o( 1, 2 ) = a12;
		o( 2, 0 ) = a20; o( 2, 1 ) = a21; o( 2, 2 ) = a22;

		return( o );
	}

	/// @brief 入力データを用いて3×1の行列を返す
	static const matrix _31( const value_type &a0, const value_type &a1, const value_type &a2 )
	{
		matrix o( 3, 1 );

		o[ 0 ] = a0;
		o[ 1 ] = a1;
		o[ 2 ] = a2;

		return( o );
	}

	/// @brief 入力データを用いて2×2の行列を返す
	static const matrix _22(
								const value_type &a00, const value_type &a01,
								const value_type &a10, const value_type &a11
							)
	{
		matrix o( 2, 2 );

		o( 0, 0 ) = a00; o( 0, 1 ) = a01;
		o( 1, 0 ) = a10; o( 1, 1 ) = a11;

		return( o );
	}

	/// @brief 入力データを用いて3×1の行列を返す
	static const matrix _21( const value_type &a0, const value_type &a1 )
	{
		matrix o( 2, 1 );

		o[ 0 ] = a0;
		o[ 1 ] = a1;

		return( o );
	}

	/// @brief 任意サイズの単位行列を返す
	static const matrix identity( size_type rows, size_type cols )
	{
		size_type size = rows < cols ? rows : cols;
		matrix o( rows, cols );
		for( size_type i = 0 ; i < size ; i++ )
		{
			o( i, i ) = 1;
		}
		return( o );
	}

	/// @brief 任意サイズのゼロ行列を返す
	static const matrix zero( size_type rows, size_type cols )
	{
		return( matrix( rows, cols ) );
	}

	/// @brief 符号反転した行列を返す
	matrix operator -( ) const
	{
		const matrix &m = *this;
		matrix o( size1_, size2_ );
		for( size_type i = 0 ; i < o.size( ) ; i++ )
		{
			o[ i ] = -m[ i ];
		}
		return( o );
	}

	/// @brief 転置行列を返す
	matrix t( ) const
	{
		const matrix &m = *this;
		matrix o( size2_, size1_ );
		for( size_type c = 0 ; c < o.cols( ) ; c++ )
		{
			for( size_type r = 0 ; r < o.rows( ) ; r++ )
			{
				o( r, c ) = m( c, r );
			}
		}
		return( o );
	}

	/// @brief 共役転置行列を返す
	matrix dagger( ) const
	{
		const matrix &m = *this;
		matrix o( size2_, size1_ );
		for( size_type c = 0 ; c < o.cols( ) ; c++ )
		{
			for( size_type r = 0 ; r < o.rows( ) ; r++ )
			{
				o( r, c ) = __numeric__::conjugate( m( c, r ) );
			}
		}
		return( o );
	}


	// 行列に対する演算子
	//   += 行列
	//   += 定数
	//   -= 行列
	//   -= 定数
	//   *= 行列
	//   *= 定数
	//   /= 定数

	/// @brief 行列の足し算
	//! 
	//! \f[ {\bf A} = {\bf A} + {\bf B} \f]
	//! 
	//! @param[in] m2 … 足し算を行う行列
	//! 
	//! @return 自分自身
	//! 
	template < class TT, class AAlocator >
	const matrix& operator +=( const matrix< TT, AAlocator > &m2 )
	{
		matrix &m1 = *this;
#if _CHECK_ARRAY_OPERATION_ != 0
		if( m1.size( ) != m2.size( ) )
		{
			// 足し算できません例外
			::std::cerr << "can't add arrays." << ::std::endl;
			return( *this );
		}
#endif
		for( size_type i = 0 ; i < m1.size( ) ; i++ ) m1[i] += static_cast< T >( m2[i] );
		return( m1 );
	}


	/// @brief 行列の引き算
	//! 
	//! \f[ {\bf A} = {\bf A} - {\bf B} \f]
	//! 
	//! @param[in] m2 … 引き算を行う行列
	//! 
	//! @return 自分自身
	//! 
	template < class TT, class AAlocator >
	const matrix& operator -=( const matrix< TT, AAlocator > &m2 )
	{
		matrix &m1 = *this;
#ifdef _CHECK_ARRAY_OPERATION_
		if( m1.size( ) != m2.size( ) )
		{
			// 引き算できません例外
			::std::cerr << "can't subtract matrixs." << ::std::endl;
			return( m1 );
		}
#endif
		for( size_type i = 0 ; i < m1.size( ) ; i++ ) m1[i] -= static_cast< T >( m2[i] );
		return( m1 );
	}


	/// @brief 行列の掛け算
	//! 
	//! \f[ {\bf A} = {\bf A} * {\bf B} \f]
	//! 
	//! @param[in] m2 … 右から掛ける行列
	//! 
	//! @return 自分自身
	//! 
	template < class TT, class AAlocator >
	const matrix& operator *=( const matrix< TT, AAlocator > &m2 )
	{
		matrix &m1 = *this;

#if defined( _CHECK_MATRIX_OPERATION_ ) && _CHECK_MATRIX_OPERATION_ != 0
		if( m1.cols( ) != m2.rows( ) )
		{
			// 掛け算できません例外
			::std::cerr << "can't multiply matrices." << ::std::endl;
			return( m1 );
		}
#endif

		typedef __numeric__::value_compare< __numeric__::is_complex< value_type >::value > value_compare;

		matrix< T, Allocator > mat( m1.rows( ), m2.cols( ) );

#ifdef _OPENMP
		int nCols = static_cast< int >( mat.cols( ) );

		#pragma omp parallel for schedule( guided )
		for( int c = 0 ; c < nCols ; c++ )
		{
			for( size_type t = 0 ; t < m1.cols( ) ; t++ )
			{
				value_type val = static_cast< value_type >( m2( t, c ) );
				if( !value_compare::is_zero( val ) )
				{
					pointer pm0 = &mat( 0, c );
					pointer pm1 = &m1( 0, t );
					for( size_type r = 0 ; r < mat.rows( ) ; r++ )
					{
						pm0[ r ] += pm1[ r ] * val;
					}
				}
			}
		}
#else
		size_type r, c, t;

		for( c = 0 ; c < mat.cols( ) ; c++ )
		{
			for( t = 0 ; t < m1.cols( ) ; t++ )
			{
				value_type val = static_cast< value_type >( m2( t, c ) );
				if( !value_compare::is_zero( val ) )
				{
					pointer pm0 = &mat( 0, c );
					pointer pm1 = &m1( 0, t );
					for( r = 0 ; r < mat.rows( ) ; r++ )
					{
						pm0[ r ] += pm1[ r ] * val;
					}
				}
			}
		}
#endif

		m1.swap( mat );

		return( m1 );
	}


	/// @brief 行列への定数（単位行列の定数倍）の足し算
	//! 
	//! \f[ {\bf A} = {\bf A} + val * {\bf I} \f]
	//! 
	//! @param[in] val … 単位行列の定数倍を表す数
	//! 
	//! @return 自分自身
	//! 
	const matrix& operator +=( typename type_trait< T >::value_type val )
	{
		matrix &m = *this;
		size_type i, size = m.rows( ) < m.cols( ) ? m.rows( ) : m.cols( );
		for( i = 0 ; i < size ; i++ ) m( i, i ) += val;
		return( m );
	}


	/// @brief 行列への定数（単位行列の定数倍）の引き算
	//! 
	//! \f[ {\bf A} = {\bf A} - val * {\bf I} \f]
	//! 
	//! @param[in] val … 単位行列の定数倍を表す数
	//! 
	//! @return 自分自身
	//! 
	const matrix& operator -=( typename type_trait< T >::value_type val )
	{
		matrix &m = *this;
		size_type i, size = m.rows( ) < m.cols( ) ? m.rows( ) : m.cols( );
		for( i = 0 ; i < size ; i++ ) m( i, i ) -= val;
		return( m );
	}


	/// @brief 行列への定数（単位行列の定数倍）の掛け算
	//! 
	//! \f[ {\bf A} = {\bf A} * val \f]
	//! 
	//! @param[in] val … 行列を定数倍する数
	//! 
	//! @return 自分自身
	//! 
	const matrix& operator *=( typename type_trait< T >::value_type val )
	{
		matrix &m = *this;
		for( size_type i = 0 ; i < m.size( ) ; i++ ) m[i] *= val;
		return( m );
	}


	/// @brief 行列への定数（単位行列の定数倍）の割り算
	//! 
	//! \f[ {\bf A} = {\bf A} / val \f]
	//! 
	//! @param[in] val … 行列を割る定数
	//! 
	//! @return 自分自身
	//! 
	const matrix& operator /=( typename type_trait< T >::value_type val )
	{
		matrix &m = *this;
#if defined( _CHECK_MATRIX_OPERATION_ ) && _CHECK_MATRIX_OPERATION_ != 0
		if( val == value_type( 0 ) )
		{
			// ゼロ除算発生
			::std::cerr << "zero division occured." << ::std::endl;
			return( m );
		}
#endif
		for( size_type i = 0 ; i < m.size( ) ; i++ ) m[i] /= val;
		return( m );
	}


	/// @brief 2つの行列が等しい（全要素の差が delta 未満）かどうかを判定する
	//! 
	//! @param[in] a     … 比較対象の行列
	//! @param[in] delta … 許容誤差
	//! 
	//! @retval true  … 2つの行列が等しい場合
	//! @retval false … 2つの行列が異なる場合
	//! 
	bool is_equal( const matrix &a, const double delta )
	{
		typedef __numeric__::value_compare< __numeric__::is_complex< value_type >::value > value_compare;

		if( rows( ) != a.rows( ) || cols( ) != a.cols( ) )
		{
			return( false );
		}

		for( size_type i = 0 ; i < base::size( ) ; i++ )
		{
			if( !value_compare::eq( operator []( i ), a[ i ], delta ) )
			{
				return( false );
			}
		}

		return( true );
	}

	/// @brief 2つの行列が等しい（全要素が同じ値を持つ）かどうかを判定する
	//! 
	//! @param[in] a … 右辺値
	//! 
	//! @retval true  … 2つの行列が等しい場合
	//! @retval false … 2つの行列が異なる場合
	//! 
	bool operator ==( const matrix &a ) const
	{
		if( rows( ) != a.rows( ) || cols( ) != a.cols( ) )
		{
			return( false );
		}

		for( size_type i = 0 ; i < base::size( ) ; i++ )
		{
			if( operator []( i ) != a[ i ] )
			{
				return( false );
			}
		}

		return( true );
	}

	/// @brief 2つの行列が等しくない（どれか１つでも異なる要素を持つ）かどうかを判定する
	//! 
	//! @param[in] a … 右辺値
	//! 
	//! @retval true  … 2つの行列が異なる場合
	//! @retval false … 2つの行列が等しい場合
	//! 
	bool operator !=( const matrix &a ) const { return( !( *this == a ) ); }


public:
	/// @brief 要素の型が異なる行列を代入する
	//! 
	//! コピー元である行列 o と全く同じ行列を作成する．
	//! コピー先（ここでは自分自身）の要素数が o と異なる場合は，自動的にサイズを調整する．
	//! コピー元とコピー先でデータ型（array< data > の data）が異なる場合の代入を行う．
	//! 
	//! @param[in] o  … コピー元の行列
	//! 
	//! @return 自分自身
	//! 
	template < class TT, class AAlocator >
	const matrix& operator =( const matrix< TT, AAlocator > &o )
	{
		base::operator =( o );

		if( base::empty( ) )
		{
			size1_ = size2_ = 0;
		}
		else
		{
			size1_ = o.size1( );
			size2_ = o.size2( );
		}

		return( *this );
	}

	/// @brief 要素の型が同じ行列を代入する
	//! 
	//! コピー元である行列 o と全く同じ行列を作成する．
	//! コピー先（ここでは自分自身）の要素数が o と異なる場合は，自動的にサイズを調整する．
	//! 
	//! @param[in] o  … コピー元の行列
	//! 
	//! @return 自分自身
	//! 
	const matrix< T, Allocator >& operator =( const matrix< T, Allocator > &o )
	{
		if( this == &o ) return( *this );

		base::operator =( o );

		if( base::empty( ) )
		{
			size1_ = size2_ = 0;
		}
		else
		{
			size1_ = o.size1( );
			size2_ = o.size2( );
		}

		return( *this );
	}


// 要素へのアクセス
private:
	/// @brief index で示される位置の要素のポインタを返す
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示すポインタ
	//! 
	pointer paccess( size_type r, size_type c )
	{
		return( base::data_ + r + c * size1_ );
	}

	/// @brief index で示される位置の要素の const ポインタを返す
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示す const ポインタ
	//! 
	const_pointer paccess( size_type r, size_type c ) const
	{
		return( base::data_ + r + c * size1_ );
	}


public:
	/// @brief r 行 c 列 で示される位置の要素の参照を返す
	//!
	//! DEBUG マクロを有効にした（NDEBUGマクロを定義しない）場合は，指定された r 行 c 列 が有効な範囲内にあるかをチェックする
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示す参照
	//! 
	reference at( size_type r, size_type c )
	{
		_CHECK_ACCESS_VIOLATION2U_( r, c )
		return( base::data_[ r + c * size1_ ] );
	}

	/// @brief r 行 c 列 で示される位置の要素の const 参照を返す
	//!
	//! DEBUG マクロを有効にした（NDEBUGマクロを定義しない）場合は，指定された r 行 c 列 が有効な範囲内にあるかをチェックする
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示す const 参照
	//! 
	const_reference at( size_type r, size_type c ) const
	{
		_CHECK_ACCESS_VIOLATION2U_( r, c )
		return( base::data_[ r + c * size1_ ] );
	}

	/// @brief r 行 c 列 で示される位置の要素の参照を返す
	//!
	//! DEBUG マクロを有効にした（NDEBUGマクロを定義しない）場合は，指定された r 行 c 列 が有効な範囲内にあるかをチェックする
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示す参照
	//! 
	reference operator ()( size_type r, size_type c )
	{
		_CHECK_ACCESS_VIOLATION2U_( r, c )
		return( base::data_[ r + c * size1_ ] );
	}

	/// @brief r 行 c 列 で示される位置の要素の const 参照を返す
	//!
	//! DEBUG マクロを有効にした（NDEBUGマクロを定義しない）場合は，指定された r 行 c 列 が有効な範囲内にあるかをチェックする
	//! 
	//! @param[in] r … 行位置
	//! @param[in] c … 列位置
	//! 
	//! @return 指定された要素を示す const 参照
	//! 
	const_reference operator ()( size_type r, size_type c ) const
	{
		_CHECK_ACCESS_VIOLATION2U_( r, c )
		return( base::data_[ r + c * size1_ ] );
	}


public:
	/// @brief ディフォルトコンストラクタ．0 行 0 列 の行列を作成する
	matrix( ) : base( ), size2_( 0 ), size1_( 0 ) {}

	/// @brief アロケータ a のコピーを利用し，0 行 0 列 の行列を作成する
	explicit matrix( const Allocator &a ) : base( a ), size2_( 0 ), size1_( 0 ) {}


	/// @brief 要素数 rnum 行 cnum 列 の行列を作成し，デフォルト値で要素を初期化する
	matrix( size_type rnum, size_type cnum ) : base( rnum * cnum ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}

	/// @brief 要素数 rnum 行 cnum 列 でアロケータ a のコピーを利用した行列を作成し，デフォルト値で要素を初期化する
	matrix( size_type rnum, size_type cnum, const Allocator &a ) : base( rnum * cnum, a ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}


	/// @brief 要素数 rnum 行 cnum 列 の行列を作成し，全要素を値 val で要素を初期化する
	matrix( size_type rnum, size_type cnum, const T &val ) : base( rnum * cnum, val ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}

	/// @brief 要素数 rnum 行 cnum 列 でアロケータ a のコピーを利用した行列を作成し，全要素を値 val で要素を初期化する
	matrix( size_type rnum, size_type cnum, const T &val, const Allocator &a ) : base( rnum * cnum, val, a ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}




	/// @brief ptr が指すメモリ領域に，要素数 rnum 行 cnum 列 の行列を作成する（ptr が指す先の利用可能なメモリ量は mem_available ）
	matrix( size_type rnum, size_type cnum, pointer ptr, size_type mem_available ) : base( rnum * cnum, ptr, mem_available ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}

	/// @brief ptr が指すメモリ領域に，要素数 rnum 行 cnum 列 の行列を作成し，全要素を値 val で要素を初期化する（ptr が指す先の利用可能なメモリ量は mem_available ）
	matrix( size_type rnum, size_type cnum, const T &val, pointer ptr, size_type mem_available ) : base( rnum * cnum, val, ptr, mem_available ), size2_( cnum ), size1_( rnum )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}


	/// @brief 他の行列で要素の型が異なるものから同じサイズの行列を作成する
	//!
	//! @attention 異なる要素型間でデータの変換が可能でなくてはならない
	//!
	template < class TT, class AAlocator >
	matrix( const matrix< TT, AAlocator > &o ) : base( o ), size2_( o.size2( ) ), size1_( o.size1( ) )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}


	/// @brief 他の行列で同じ要素型のものを用いて初期化する
	matrix( const matrix< T, Allocator > &o ) : base( o ), size2_( o.size2_ ), size1_( o.size1_ )
	{
		if( base::empty( ) ) size1_ = size2_ = 0;
	}
};



/// @brief 指定されたストリームに，行列内の要素を整形して出力する
//! 
//! @param[in,out] out … 入力と出力を行うストリーム
//! @param[in]     m   … 出力する行列
//! 
//! @return 入力されたストリーム
//! 
//! @code 出力例
//! 1, 4, 7
//! 2, 5, 8
//! 3, 6, 9
//! @endcode
//! 
template < class T, class Allocator >
inline ::std::ostream &operator <<( ::std::ostream &out, const matrix< T, Allocator > &m )
{
	typename matrix< T, Allocator >::size_type r, c;
	for( r = 0 ; r < m.rows( ) ; r++ )
	{
		if( r != 0 )
		{
			out << ::std::endl;
		}
		for( c = 0 ; c < m.cols( ) ; c++ )
		{
			out << m( r, c );
			if( c != m.cols( ) - 1 ) out << ", ";
		}
	}

	return( out );
}


/************************************************************************************************************
**
**      行列に対する演算子
**        行列 + 行列
**        行列 + 定数
**        定数 + 行列
**
**        行列 - 行列
**        行列 - 定数
**        定数 - 行列
**
**        行列 * 行列
**        行列 * 定数
**        定数 * 行列
**
**        行列 / 定数
**
************************************************************************************************************/


/// @brief 足し算
//! 
//! @param[in] m1 … 左辺値
//! @param[in] m2 … 右辺値
//! 
//! @return 足し算結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator +( const matrix< T, Allocator > &m1, const matrix< T, Allocator > &m2 )
{
	return( matrix< T, Allocator >( m1 ) += m2 );
}


/// @brief 引き算
//! 
//! @param[in] m1 … 左辺値
//! @param[in] m2 … 右辺値
//! 
//! @return 引き算結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator -( const matrix< T, Allocator > &m1, const matrix< T, Allocator > &m2 )
{
	return( matrix< T, Allocator >( m1 ) -= m2 );
}


/// @brief 掛け算
//! 
//! @param[in] m1 … 左辺値
//! @param[in] m2 … 右辺値
//! 
//! @return 掛け算結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator *( const matrix< T, Allocator > &m1, const matrix< T, Allocator > &m2 )
{
	return( matrix< T, Allocator >( m1 ) *= m2 );
}


/// @brief 単位行列の定数倍との足し算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] m   … 行列
//! @param[in] val … 定数
//! 
//! @return 左辺値の行列に単位行列の定数倍を足し算した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator +( const matrix< T, Allocator > &m, typename type_trait< T >::value_type val )
{
	return( matrix< T, Allocator >( m ) += val );
}

/// @brief 定数との足し算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] val … 定数
//! @param[in] m   … 行列
//! 
//! @return 右辺値の行列に単位行列の定数倍を足し算した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator +( typename matrix< T, Allocator >::value_type val, const matrix< T, Allocator > &m )
{
	return( matrix< T, Allocator >( m ) += val );
}



/// @brief 定数との引き算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] m   … 行列
//! @param[in] val … 定数
//! 
//! @return 左辺値の行列から，単位行列を定数倍したものを引き算した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator -( const matrix< T, Allocator > &m, typename type_trait< T >::value_type val )
{
	return( matrix< T, Allocator >( m ) -= val );
}

/// @brief 定数との引き算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] val … 定数
//! @param[in] m   … 行列
//! 
//! @return 単位行列を定数倍したものから右辺値の行列を引き算した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator -( typename matrix< T, Allocator >::value_type val, const matrix< T, Allocator > &m )
{
	return( matrix< T, Allocator >( m ) -= val );
}


/// @brief 定数との掛け算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] m   … 行列
//! @param[in] val … 定数
//! 
//! @return 左辺値の行列を定数倍した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator *( const matrix< T, Allocator > &m, typename type_trait< T >::value_type val )
{
	return( matrix< T, Allocator >( m ) *= val );
}

/// @brief 定数との掛け算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] val … 定数
//! @param[in] m   … 行列
//! 
//! @return 右辺値の行列を定数倍した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator *( typename matrix< T, Allocator >::value_type val, const matrix< T, Allocator > &m )
{
	return( matrix< T, Allocator >( m ) *= val );
}


// @brief 定数との割り算
//! 
//! 詳細な説明や関数の使用例を書く
//! 
//! @param[in] m   … 行列
//! @param[in] val … 定数
//! 
//! @return 左辺値の行列を定数で割り算した結果
//! 
template < class T, class Allocator >
inline matrix< T, Allocator > operator /( const matrix< T, Allocator > &m, typename type_trait< T >::value_type val )
{
	return( matrix< T, Allocator >( m ) /= val );
}


// mist名前空間の終わり
_MIST_END

#endif // __INCLUDE_MIST_MATRIX__
