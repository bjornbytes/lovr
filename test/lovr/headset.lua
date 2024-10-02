group('headset', function()
  test('isVisible', function()
    expect(lovr.headset.isVisible()).to.be.a('boolean')
  end)

  test('isFocused', function()
    expect(lovr.headset.isFocused()).to.be.a('boolean')
  end)

  test('isMounted', function()
    expect(lovr.headset.isMounted()).to.be.a('boolean')
  end)
end)
